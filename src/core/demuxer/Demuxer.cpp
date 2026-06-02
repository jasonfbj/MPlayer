#include "core/demuxer/Demuxer.h"
#include <cstring>

Demuxer::Demuxer() = default;

Demuxer::~Demuxer() {
    close();
}

bool Demuxer::open(const std::string& url) {
    NetworkConfig defaultConfig;
    return open(url, defaultConfig);
}

bool Demuxer::open(const std::string& url, const NetworkConfig& config) {
    if (opened_) close();

    netConfig_ = config;
    currentUrl_ = url;
    setConnState(ConnectionState::Connecting, "Connecting to " + url);

    formatCtx_ = avformat_alloc_context();
    if (!formatCtx_) {
        setConnState(ConnectionState::Failed, "Failed to allocate format context");
        return false;
    }

    // 网络流设置
    if (url.find("rtmp://") == 0 || url.find("http://") == 0 || url.find("rtsp://") == 0) {
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "timeout", std::to_string(config.timeoutUs).c_str(), 0);
        av_dict_set(&opts, "buffer_size", std::to_string(config.bufferSize).c_str(), 0);
        av_dict_set(&opts, "max_delay", std::to_string(config.maxDelay).c_str(), 0);
        av_dict_set(&opts, "rtmp_live", "live", 0);
        av_dict_set(&opts, "fflags", "nobuffer", 0);

        int ret = avformat_open_input(&formatCtx_, url.c_str(), nullptr, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            avformat_free_context(formatCtx_);
            formatCtx_ = nullptr;
            setConnState(ConnectionState::Failed, "Failed to open input: " + url);
            return false;
        }
    } else {
        if (avformat_open_input(&formatCtx_, url.c_str(), nullptr, nullptr) < 0) {
            avformat_free_context(formatCtx_);
            formatCtx_ = nullptr;
            setConnState(ConnectionState::Failed, "Failed to open input: " + url);
            return false;
        }
    }

    if (avformat_find_stream_info(formatCtx_, nullptr) < 0) {
        close();
        setConnState(ConnectionState::Failed, "Failed to find stream info");
        return false;
    }

    // 查找音视频流
    for (unsigned int i = 0; i < formatCtx_->nb_streams; i++) {
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            videoStreamIndex_ < 0) {
            videoStreamIndex_ = i;
        }
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
            audioStreamIndex_ < 0) {
            audioStreamIndex_ = i;
        }
    }

    opened_ = true;
    eof_ = false;
    setConnState(ConnectionState::Connected, "Connected to " + url);
    return true;
}

void Demuxer::close() {
    if (formatCtx_) {
        avformat_close_input(&formatCtx_);
        formatCtx_ = nullptr;
    }
    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;
    opened_ = false;
    eof_ = false;
}

bool Demuxer::readPacket(AVPacket* packet) {
    if (!opened_ || eof_) return false;

    int ret = av_read_frame(formatCtx_, packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF || avio_feof(formatCtx_->pb)) {
            eof_ = true;
            return false;
        }
        // Network error on a stream — attempt reconnection
        if (isStream()) {
            return reconnectAndRead(packet);
        }
        return false;
    }
    return true;
}

bool Demuxer::reconnectAndRead(AVPacket* packet) {
    for (int attempt = 1; attempt <= netConfig_.maxRetries; ++attempt) {
        setConnState(ConnectionState::Reconnecting,
            "Reconnection attempt " + std::to_string(attempt) + "/" + std::to_string(netConfig_.maxRetries));

        // Exponential backoff delay
        int delayMs = netConfig_.retryBaseDelayMs * (1 << (attempt - 1));
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

        // Close current context and retry
        if (formatCtx_) {
            avformat_close_input(&formatCtx_);
            formatCtx_ = nullptr;
        }
        opened_ = false;
        videoStreamIndex_ = -1;
        audioStreamIndex_ = -1;

        if (open(currentUrl_, netConfig_)) {
            // Successfully reconnected, try to read a packet
            int ret = av_read_frame(formatCtx_, packet);
            if (ret >= 0) {
                return true;
            }
        }
    }

    setConnState(ConnectionState::Failed, "Reconnection failed after " +
        std::to_string(netConfig_.maxRetries) + " attempts");
    return false;
}

AVCodecParameters* Demuxer::getVideoParams() const {
    if (videoStreamIndex_ < 0) return nullptr;
    return formatCtx_->streams[videoStreamIndex_]->codecpar;
}

AVCodecParameters* Demuxer::getAudioParams() const {
    if (audioStreamIndex_ < 0) return nullptr;
    return formatCtx_->streams[audioStreamIndex_]->codecpar;
}

bool Demuxer::seek(double seconds) {
    if (!opened_) return false;
    if (isStream()) return false;

    int64_t ts = static_cast<int64_t>(seconds * AV_TIME_BASE);
    int ret = av_seek_frame(formatCtx_, -1, ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) return false;

    eof_ = false;
    return true;
}

bool Demuxer::isStream() const {
    if (!opened_ || !formatCtx_) return false;
    return formatCtx_->iformat && (
        std::strstr(formatCtx_->iformat->name, "rtmp") != nullptr ||
        formatCtx_->pb == nullptr ||
        !formatCtx_->pb->seekable
    );
}

double Demuxer::duration() const {
    if (!opened_ || !formatCtx_) return 0.0;
    return static_cast<double>(formatCtx_->duration) / AV_TIME_BASE;
}

void Demuxer::setConnectionCallback(ConnectionCallback cb) {
    connCb_ = std::move(cb);
}

ConnectionState Demuxer::connectionState() const {
    return connState_.load();
}
