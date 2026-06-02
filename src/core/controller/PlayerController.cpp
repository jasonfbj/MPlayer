#include "core/controller/PlayerController.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

static const size_t MAX_PACKET_QUEUE_SIZE = 500;
static const size_t MAX_FRAME_QUEUE_SIZE = 30;

PlayerController::PlayerController()
    : demuxer_(std::make_unique<Demuxer>()) {
    videoPacketQueue_.setMaxSize(MAX_PACKET_QUEUE_SIZE);
    audioPacketQueue_.setMaxSize(MAX_PACKET_QUEUE_SIZE);
    videoFrameQueue_.setMaxSize(MAX_FRAME_QUEUE_SIZE);
    audioFrameQueue_.setMaxSize(MAX_FRAME_QUEUE_SIZE);
}

PlayerController::~PlayerController() {
    close();
}

bool PlayerController::open(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != Idle && state_ != Stopped) {
        close();
    }

    if (!demuxer_->open(url)) {
        if (errorCb_) errorCb_("Failed to open: " + url);
        return false;
    }

    currentUrl_ = url;

    if (demuxer_->getVideoStreamIndex() >= 0) {
        videoDecoder_ = DecoderFactory::createVideoDecoder(
            demuxer_->getVideoParams(),
            DecoderFactory::DecoderType::Auto
        );
        if (!videoDecoder_) {
            if (errorCb_) errorCb_("Failed to create video decoder");
            return false;
        }
    }

    if (demuxer_->getAudioStreamIndex() >= 0) {
        audioDecoder_ = DecoderFactory::createAudioDecoder(
            demuxer_->getAudioParams()
        );
    }

    if (audioOutput_ && demuxer_->getAudioStreamIndex() >= 0) {
        auto* aparams = demuxer_->getAudioParams();
        audioOutput_->init(aparams->sample_rate, aparams->ch_layout.nb_channels, 2);
    }

    setState(Stopped);
    return true;
}

bool PlayerController::open(const std::string& url, const NetworkConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != Idle && state_ != Stopped) {
        close();
    }

    if (!demuxer_->open(url, config)) {
        if (errorCb_) errorCb_("Failed to open: " + url);
        return false;
    }

    currentUrl_ = url;

    if (demuxer_->getVideoStreamIndex() >= 0) {
        videoDecoder_ = DecoderFactory::createVideoDecoder(
            demuxer_->getVideoParams(),
            DecoderFactory::DecoderType::Auto
        );
        if (!videoDecoder_) {
            if (errorCb_) errorCb_("Failed to create video decoder");
            return false;
        }
    }

    if (demuxer_->getAudioStreamIndex() >= 0) {
        audioDecoder_ = DecoderFactory::createAudioDecoder(
            demuxer_->getAudioParams()
        );
    }

    if (audioOutput_ && demuxer_->getAudioStreamIndex() >= 0) {
        auto* aparams = demuxer_->getAudioParams();
        audioOutput_->init(aparams->sample_rate, aparams->ch_layout.nb_channels, 2);
    }

    setState(Stopped);
    return true;
}

void PlayerController::close() {
    stop();

    videoPacketQueue_.finish();
    audioPacketQueue_.finish();
    videoFrameQueue_.finish();
    audioFrameQueue_.finish();

    demuxer_->close();
    videoDecoder_.reset();
    audioDecoder_.reset();

    videoPacketQueue_.clear();
    audioPacketQueue_.clear();
    videoFrameQueue_.clear();
    audioFrameQueue_.clear();

    setState(Idle);
}

void PlayerController::play() {
    if (state_ == Playing) return;
    if (state_ != Paused && state_ != Stopped) return;

    if (state_ == Stopped) {
        running_ = true;
        videoPacketQueue_.clear();
        audioPacketQueue_.clear();
        videoFrameQueue_.clear();
        audioFrameQueue_.clear();

        if (!demuxer_->isOpened() && !currentUrl_.empty()) {
            demuxer_->open(currentUrl_);
            if (videoDecoder_) videoDecoder_->flush();
            if (audioDecoder_) audioDecoder_->flush();
        }

        readThread_ = std::thread(&PlayerController::readThread, this);
        videoDecodeThread_ = std::thread(&PlayerController::videoDecodeThread, this);
        if (audioDecoder_) {
            audioDecodeThread_ = std::thread(&PlayerController::audioDecodeThread, this);
        }
    }

    if (audioOutput_) audioOutput_->start();
    setState(Playing);
}

void PlayerController::pause() {
    if (state_ != Playing) return;
    if (audioOutput_) audioOutput_->stop();
    setState(Paused);
}

void PlayerController::stop() {
    running_ = false;

    videoPacketQueue_.finish();
    audioPacketQueue_.finish();

    if (readThread_.joinable()) readThread_.join();
    if (videoDecodeThread_.joinable()) videoDecodeThread_.join();
    if (audioDecodeThread_.joinable()) audioDecodeThread_.join();

    if (audioOutput_) audioOutput_->stop();
    setState(Stopped);
}

void PlayerController::seek(double seconds) {
    if (!demuxer_->isOpened()) return;

    videoPacketQueue_.clear();
    audioPacketQueue_.clear();
    videoFrameQueue_.clear();
    audioFrameQueue_.clear();

    if (videoDecoder_) videoDecoder_->flush();
    if (audioDecoder_) audioDecoder_->flush();

    demuxer_->seek(seconds);
    currentPosition_ = seconds;
}

void PlayerController::setSpeed(float speed) {
    speed_ = speed;
}

void PlayerController::setVolume(float volume) {
    if (audioOutput_) {
        audioOutput_->setVolume(volume);
    }
}

double PlayerController::duration() const {
    return demuxer_ ? demuxer_->duration() : 0.0;
}

double PlayerController::currentPosition() const {
    return currentPosition_;
}

void PlayerController::setRenderer(std::unique_ptr<IRenderer> renderer) {
    renderer_ = std::move(renderer);
}

void PlayerController::setAudioOutput(std::unique_ptr<IAudioOutput> audioOutput) {
    audioOutput_ = std::move(audioOutput);
}

VideoInfo PlayerController::getVideoInfo() const {
    VideoInfo info;
    if (!demuxer_ || !demuxer_->isOpened()) return info;

    auto* vparams = demuxer_->getVideoParams();
    if (vparams) {
        info.codecName = avcodec_get_name(vparams->codec_id);
        info.width = vparams->width;
        info.height = vparams->height;
        info.duration = demuxer_->duration();
        info.bitrate = vparams->bit_rate;

        if (demuxer_->getVideoStreamIndex() >= 0) {
            auto* stream = demuxer_->getFormatContext()->streams[demuxer_->getVideoStreamIndex()];
            if (stream->avg_frame_rate.den > 0) {
                info.frameRate = av_q2d(stream->avg_frame_rate);
            }
        }
    }
    return info;
}

void PlayerController::readThread() {
    AVPacket* packet = av_packet_alloc();
    while (running_) {
        if (!demuxer_->readPacket(packet)) {
            break;
        }

        if (packet->stream_index == demuxer_->getVideoStreamIndex()) {
            AVPacket* pkt = av_packet_alloc();
            av_packet_ref(pkt, packet);
            if (!videoPacketQueue_.push(pkt)) {
                av_packet_free(&pkt);
            }
        } else if (packet->stream_index == demuxer_->getAudioStreamIndex()) {
            AVPacket* pkt = av_packet_alloc();
            av_packet_ref(pkt, packet);
            if (!audioPacketQueue_.push(pkt)) {
                av_packet_free(&pkt);
            }
        }

        av_packet_unref(packet);
    }
    av_packet_free(&packet);
}

void PlayerController::videoDecodeThread() {
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = nullptr;

    while (running_) {
        if (!videoPacketQueue_.pop(packet, 100)) {
            continue;
        }

        if (videoDecoder_->decode(packet, frame)) {
            VideoFrame vf;
            vf.width = frame->width;
            vf.height = frame->height;

            AVRational timeBase = demuxer_->getFormatContext()->
                streams[demuxer_->getVideoStreamIndex()]->time_base;
            vf.pts = frame->pts * av_q2d(timeBase);

            if (frame->format == AV_PIX_FMT_YUV420P) {
                vf.format = VideoFrame::YUV420P;
                for (int i = 0; i < 3; i++) {
                    vf.linesize[i] = frame->linesize[i];
                    int h = (i == 0) ? frame->height : frame->height / 2;
                    int size = frame->linesize[i] * h;
                    vf.data[i].assign(frame->data[i], frame->data[i] + size);
                }
            } else if (frame->format == AV_PIX_FMT_NV12) {
                vf.format = VideoFrame::NV12;
                vf.linesize[0] = frame->linesize[0];
                vf.linesize[1] = frame->linesize[1];
                int ySize = frame->linesize[0] * frame->height;
                vf.data[0].assign(frame->data[0], frame->data[0] + ySize);
                int uvSize = frame->linesize[1] * frame->height / 2;
                vf.data[1].assign(frame->data[1], frame->data[1] + uvSize);
            }

            if (videoFrameCb_) {
                videoFrameCb_(vf);
            }

            videoFrameQueue_.push(std::move(vf));
            currentPosition_ = vf.pts;
        }

        av_packet_free(&packet);
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}

void PlayerController::audioDecodeThread() {
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = nullptr;

    while (running_) {
        if (!audioPacketQueue_.pop(packet, 100)) {
            continue;
        }

        if (audioDecoder_ && audioDecoder_->decode(packet, frame)) {
            AudioFrame af;
            af.sampleRate = frame->sample_rate;
            af.channels = frame->ch_layout.nb_channels;
            af.samples = frame->nb_samples;
            af.bytesPerSample = av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));

            AVRational timeBase = demuxer_->getFormatContext()->
                streams[demuxer_->getAudioStreamIndex()]->time_base;
            af.pts = frame->pts * av_q2d(timeBase);

            int dataSize = av_samples_get_buffer_size(nullptr, af.channels, af.samples,
                static_cast<AVSampleFormat>(frame->format), 1);
            if (dataSize > 0 && frame->data[0]) {
                af.data.assign(frame->data[0], frame->data[0] + dataSize);
                audioFrameQueue_.push(std::move(af));
            }
        }

        av_packet_free(&packet);
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}

void PlayerController::setState(State s) {
    state_ = s;
    if (stateCb_) stateCb_(s);
}

void PlayerController::setConnectionCallback(ConnectionCallback cb) {
    if (demuxer_) {
        demuxer_->setConnectionCallback(std::move(cb));
    }
}

ConnectionState PlayerController::connectionState() const {
    return demuxer_ ? demuxer_->connectionState() : ConnectionState::Disconnected;
}
