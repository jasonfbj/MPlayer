#include "core/controller/PlayerController.h"

#include <algorithm>

#ifdef _WIN32
#include "platform/windows/D3D11VAHardwareDecoder.h"
#elif defined(__ANDROID__)
#include "platform/android/MediaCodecDecoder.h"
#endif

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

extern "C" {
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
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
        void* device = nullptr;
        if (renderer_) device = renderer_->getNativeDevice();

        videoDecoder_ = DecoderFactory::createVideoDecoder(
            demuxer_->getVideoParams(),
            DecoderFactory::DecoderType::Auto,
            device
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
        setupAudioCallback();
    }

    initAudioResampler();

    setState(Stopped);
    return true;
}

bool PlayerController::open(const std::string& url, const NetworkConfig& config) {
    // Delegate to the simple overload — Demuxer::open(url) forwards to open(url, defaultConfig)
    // To avoid code duplication, just set config on demuxer and call the base open
    (void)config;  // Use NetworkConfig via Demuxer directly
    // Actually, open with specific config:
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
        void* device = nullptr;
        if (renderer_) device = renderer_->getNativeDevice();

        videoDecoder_ = DecoderFactory::createVideoDecoder(
            demuxer_->getVideoParams(),
            DecoderFactory::DecoderType::Auto,
            device
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
        setupAudioCallback();
    }

    initAudioResampler();

    setState(Stopped);
    return true;
}

void PlayerController::close() {
    stop();

    eof_ = false;
    audioResidual_.clear();
    audioResidualOffset_ = 0;

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
        eof_ = false;
        running_ = true;
        paused_ = false;
        videoPacketQueue_.clear();
        audioPacketQueue_.clear();
        videoFrameQueue_.clear();
        audioFrameQueue_.clear();

        if (!demuxer_->isOpened() && !currentUrl_.empty()) {
            demuxer_->open(currentUrl_);
        }

        // Seek to beginning so playback restarts from the start.
        // If seek fails (e.g., demuxer was still open at EOF), close and reopen.
        // Safe to call directly: readThread hasn't started yet.
        if (demuxer_->isOpened()) {
            if (!demuxer_->seek(0)) {
                demuxer_->close();
                if (!currentUrl_.empty()) {
                    demuxer_->open(currentUrl_);
                }
            }
        }

        if (videoDecoder_) videoDecoder_->flush();
        if (audioDecoder_) audioDecoder_->flush();

        readThread_ = std::thread(&PlayerController::readThread, this);
        videoDecodeThread_ = std::thread(&PlayerController::videoDecodeThread, this);
        if (audioDecoder_) {
            audioDecodeThread_ = std::thread(&PlayerController::audioDecodeThread, this);
        }
    } else if (state_ == Paused) {
        // Unpause: wake up decode threads so they resume processing
        paused_ = false;
        pauseCond_.notify_all();
    }

    if (audioOutput_) audioOutput_->start();
    setState(Playing);
}

void PlayerController::pause() {
    if (state_ != Playing) return;
    paused_ = true;
    pauseCond_.notify_all();
    if (audioOutput_) audioOutput_->stop();
    setState(Paused);
}

void PlayerController::stop() {
    running_ = false;
    paused_ = false;
    pauseCond_.notify_all();

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
    if (state_ != Playing && state_ != Paused) return;

    // Clear queues to discard stale data
    videoPacketQueue_.clear();
    audioPacketQueue_.clear();
    videoFrameQueue_.clear();
    audioFrameQueue_.clear();

    if (videoDecoder_) videoDecoder_->flush();
    if (audioDecoder_) audioDecoder_->flush();

    // Request seek on readThread — avoids blocking main thread on network I/O
    seekTarget_ = seconds;
    seekRequested_ = true;
    currentPosition_ = seconds;

    // Wake readThread from pause or blocked state to process the seek
    pauseCond_.notify_all();

    // If currently paused, briefly unpause to decode the seek target frame,
    // then re-pause after one frame is produced
    if (paused_.load()) {
        seekPauseAfterDecode_ = true;
        paused_ = false;
        pauseCond_.notify_all();
    }
}

void PlayerController::setSpeed(float speed) {
    speed_ = speed;
    if (audioResampler_) {
        audioResampler_->setSpeed(speed);
    }
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
        // Wait here while paused
        {
            std::unique_lock<std::mutex> lock(pauseMutex_);
            pauseCond_.wait(lock, [this] {
                return (!paused_.load() && !seekRequested_.load()) || !running_.load();
            });
        }
        if (!running_) break;

        // Handle seek request on the read thread — avoids blocking main thread
        if (seekRequested_.exchange(false)) {
            double target = seekTarget_.load();
            demuxer_->seek(target);
            currentPosition_ = target;
            continue;
        }

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

    // Signal EOF: finish packet queues so decode threads know to stop
    eof_ = true;
    videoPacketQueue_.finish();
    audioPacketQueue_.finish();
}

void PlayerController::videoDecodeThread() {
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = nullptr;

    // PTS-based frame pacing state
    double prevPts = -1.0;

    while (running_) {
        // Wait here while paused
        {
            std::unique_lock<std::mutex> lock(pauseMutex_);
            pauseCond_.wait(lock, [this] { return !paused_.load() || !running_.load(); });
        }
        if (!running_) break;

        if (!videoPacketQueue_.pop(packet, 100)) {
            // Packet queue finished (EOF) — exit decode loop
            if (videoPacketQueue_.isFinished()) break;
            continue;
        }

        if (videoDecoder_->decode(packet, frame)) {
            VideoFrame vf;
            vf.width = frame->width;
            vf.height = frame->height;

            AVRational timeBase = demuxer_->getFormatContext()->
                streams[demuxer_->getVideoStreamIndex()]->time_base;
            if (frame->pts != AV_NOPTS_VALUE) {
                vf.pts = frame->pts * av_q2d(timeBase);
            } else if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                vf.pts = frame->best_effort_timestamp * av_q2d(timeBase);
            } else {
                // No valid PTS — estimate from frame rate
                AVRational frameRate = demuxer_->getFormatContext()->
                    streams[demuxer_->getVideoStreamIndex()]->avg_frame_rate;
                if (frameRate.num > 0 && frameRate.den > 0) {
                    vf.pts = currentPosition_.load() + av_q2d(av_inv_q(frameRate));
                } else {
                    vf.pts = currentPosition_.load() + 1.0 / 30.0;
                }
            }

            if (videoDecoder_->isHardware()) {
                // Hardware decode path - NativeTexture
                vf.format = VideoFrame::NativeTexture;

#ifdef _WIN32
                auto* d3d11dec = dynamic_cast<D3D11VAHardwareDecoder*>(videoDecoder_.get());
                if (d3d11dec) {
                    d3d11dec->getLastNativeTexture(vf.nativeTex);
                }
#elif defined(__ANDROID__)
                auto* mcdec = dynamic_cast<MediaCodecDecoder*>(videoDecoder_.get());
                if (mcdec) {
                    mcdec->getLastNativeTexture(vf.nativeTex);
                }
#endif
            } else {
                // Software decode path - YUV data
                AVPixelFormat fmt = static_cast<AVPixelFormat>(frame->format);
                if (fmt == AV_PIX_FMT_YUV420P || fmt == AV_PIX_FMT_YUVJ420P) {
                    vf.format = VideoFrame::YUV420P;
                    for (int i = 0; i < 3; i++) {
                        vf.linesize[i] = frame->linesize[i];
                        int h = (i == 0) ? frame->height : frame->height / 2;
                        int size = frame->linesize[i] * h;
                        vf.data[i].assign(frame->data[i], frame->data[i] + size);
                    }
                } else if (fmt == AV_PIX_FMT_NV12) {
                    vf.format = VideoFrame::YUV420P;
                    vf.linesize[0] = frame->width;
                    vf.linesize[1] = frame->width / 2;
                    vf.linesize[2] = frame->width / 2;

                    int halfH = frame->height / 2;

                    vf.data[0].resize(frame->width * frame->height);
                    for (int y = 0; y < frame->height; y++) {
                        memcpy(vf.data[0].data() + y * frame->width,
                               frame->data[0] + y * frame->linesize[0],
                               frame->width);
                    }

                    vf.data[1].resize((frame->width / 2) * halfH);
                    vf.data[2].resize((frame->width / 2) * halfH);
                    int uvLinesize = frame->linesize[1];
                    for (int y = 0; y < halfH; y++) {
                        const uint8_t* uvRow = frame->data[1] + y * uvLinesize;
                        uint8_t* uRow = vf.data[1].data() + y * (frame->width / 2);
                        uint8_t* vRow = vf.data[2].data() + y * (frame->width / 2);
                        for (int x = 0; x < frame->width / 2; x++) {
                            uRow[x] = uvRow[x * 2];
                            vRow[x] = uvRow[x * 2 + 1];
                        }
                    }
                } else if (fmt == AV_PIX_FMT_YUV422P || fmt == AV_PIX_FMT_YUVJ422P) {
                    vf.format = VideoFrame::YUV420P;
                    vf.linesize[0] = frame->linesize[0];
                    vf.data[0].assign(frame->data[0], frame->data[0] + frame->linesize[0] * frame->height);
                    int halfH = frame->height / 2;
                    vf.linesize[1] = frame->linesize[1];
                    vf.data[1].resize(frame->linesize[1] * halfH);
                    for (int y = 0; y < halfH; y++) {
                        memcpy(vf.data[1].data() + y * frame->linesize[1],
                               frame->data[1] + y * 2 * frame->linesize[1],
                               frame->linesize[1]);
                    }
                    vf.linesize[2] = frame->linesize[2];
                    vf.data[2].resize(frame->linesize[2] * halfH);
                    for (int y = 0; y < halfH; y++) {
                        memcpy(vf.data[2].data() + y * frame->linesize[2],
                               frame->data[2] + y * 2 * frame->linesize[2],
                               frame->linesize[2]);
                    }
                } else {
                    av_packet_free(&packet);
                    av_frame_unref(frame);
                    continue;
                }
            }

            // PTS-based frame pacing: wait between frames to match video timing
            // Uses condition_variable so pause/stop can interrupt the wait
            double pts = vf.pts;
            if (prevPts >= 0) {
                double ptsDiff = pts - prevPts;
                if (ptsDiff > 0 && ptsDiff < 1.0) {
                    double delay = ptsDiff / speed_.load();
                    if (delay > 0.001) {
                        std::unique_lock<std::mutex> lock(pauseMutex_);
                        pauseCond_.wait_for(lock,
                            std::chrono::duration<double>(delay),
                            [this] { return paused_.load() || !running_.load(); });
                    }
                }
            }
            prevPts = pts;

            if (videoFrameCb_) {
                videoFrameCb_(vf);
            }

            // Cache latest frame for screenshot
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                lastFrame_ = vf;
            }

            videoFrameQueue_.push(std::move(vf));
            currentPosition_ = pts;

            // If seek-while-paused, pause again after producing one frame
            if (seekPauseAfterDecode_.exchange(false)) {
                prevPts = -1.0;  // Reset pacing state for next unpause
                paused_ = true;
                if (audioOutput_) audioOutput_->stop();
                setState(Paused);
            }
        }

        av_packet_free(&packet);
        av_frame_unref(frame);
    }

    // Signal that no more video frames will be produced
    videoFrameQueue_.finish();
    av_frame_free(&frame);
}

void PlayerController::audioDecodeThread() {
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = nullptr;

    // SwrContext for converting any input format → S16 interleaved
    SwrContext* swrCtx = nullptr;
    AVSampleFormat lastFmt = AV_SAMPLE_FMT_NONE;
    int lastChannels = 0;
    int lastSampleRate = 0;

    while (running_) {
        // Wait here while paused
        {
            std::unique_lock<std::mutex> lock(pauseMutex_);
            pauseCond_.wait(lock, [this] { return !paused_.load() || !running_.load(); });
        }
        if (!running_) break;

        if (!audioPacketQueue_.pop(packet, 100)) {
            // Packet queue finished (EOF) — exit decode loop
            if (audioPacketQueue_.isFinished()) break;
            continue;
        }

        if (audioDecoder_ && audioDecoder_->decode(packet, frame)) {
            AudioFrame af;
            af.sampleRate = frame->sample_rate;
            af.channels = frame->ch_layout.nb_channels;
            af.samples = frame->nb_samples;
            af.bytesPerSample = 2;  // Always output S16 (matches WASAPI init)

            AVRational timeBase = demuxer_->getFormatContext()->
                streams[demuxer_->getAudioStreamIndex()]->time_base;
            af.pts = (frame->pts != AV_NOPTS_VALUE) ?
                frame->pts * av_q2d(timeBase) : 0.0;

            // Speed change via atempo filter (outputs S16 via sink config)
            if (audioResampler_ && speed_ != 1.0f) {
                std::vector<uint8_t> resampled;
                if (audioResampler_->process(frame, resampled)) {
                    af.data = std::move(resampled);
                    af.samples = static_cast<int>(af.data.size()) /
                        (af.channels * af.bytesPerSample);
                }
            } else {
                // Convert to S16 interleaved using swr_convert
                AVSampleFormat fmt = static_cast<AVSampleFormat>(frame->format);

                // (Re)create SwrContext if input format changed
                if (!swrCtx || fmt != lastFmt || af.channels != lastChannels
                    || af.sampleRate != lastSampleRate) {
                    if (swrCtx) swr_free(&swrCtx);

                    AVChannelLayout outLayout;
                    av_channel_layout_default(&outLayout, af.channels);
                    AVChannelLayout inLayout;
                    av_channel_layout_copy(&inLayout, &frame->ch_layout);

                    int ret = swr_alloc_set_opts2(&swrCtx,
                        &outLayout, AV_SAMPLE_FMT_S16, af.sampleRate,
                        &inLayout, fmt, af.sampleRate,
                        0, nullptr);

                    if (ret >= 0 && swrCtx) {
                        swr_init(swrCtx);
                    }

                    av_channel_layout_uninit(&outLayout);
                    av_channel_layout_uninit(&inLayout);

                    lastFmt = fmt;
                    lastChannels = af.channels;
                    lastSampleRate = af.sampleRate;
                }

                if (swrCtx) {
                    int bufSize = af.samples * af.channels * af.bytesPerSample;
                    af.data.resize(bufSize);
                    uint8_t* outBuf = af.data.data();
                    int converted = swr_convert(swrCtx, &outBuf, af.samples,
                        const_cast<const uint8_t**>(frame->extended_data),
                        af.samples);
                    if (converted > 0) {
                        af.samples = converted;
                        af.data.resize(converted * af.channels * af.bytesPerSample);
                    } else {
                        af.data.clear();
                    }
                }
            }

            if (!af.data.empty()) {
                audioFrameQueue_.push(std::move(af));
            }
        }

        av_packet_free(&packet);
        av_frame_unref(frame);
    }

    if (swrCtx) swr_free(&swrCtx);
    audioFrameQueue_.finish();
    av_frame_free(&frame);
}

void PlayerController::setState(State s) {
    state_ = s;
    if (stateCb_) stateCb_(s);
}

bool PlayerController::isPlaybackComplete() const {
    if (!eof_.load()) return false;
    if (!videoFrameQueue_.isFinished()) return false;
    // For audio-only or audio+video files, also wait for audio to finish
    if (audioDecoder_ && !audioFrameQueue_.isFinished()) return false;
    return true;
}

void PlayerController::setConnectionCallback(ConnectionCallback cb) {
    if (demuxer_) {
        demuxer_->setConnectionCallback(std::move(cb));
    }
}

ConnectionState PlayerController::connectionState() const {
    return demuxer_ ? demuxer_->connectionState() : ConnectionState::Disconnected;
}

void PlayerController::initAudioResampler() {
    if (!demuxer_ || demuxer_->getAudioStreamIndex() < 0) return;

    auto* aparams = demuxer_->getAudioParams();
    if (!aparams) return;

    audioResampler_ = std::make_unique<AudioResampler>();
    if (!audioResampler_->init(
        aparams->sample_rate,
        aparams->ch_layout.nb_channels,
        static_cast<AVSampleFormat>(aparams->format))) {
        audioResampler_.reset();
    }
}

bool PlayerController::captureFrame(const std::string& savePath) {
    VideoFrame frame;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        if (lastFrame_.format == VideoFrame::Format::NativeTexture) {
            // Hardware decoded frames not supported for screenshot (requires GPU readback)
            return false;
        }
        frame = lastFrame_;
    }

    if (frame.width <= 0 || frame.height <= 0) return false;

    // Determine source pixel format
    AVPixelFormat srcFmt = AV_PIX_FMT_NONE;
    if (frame.format == VideoFrame::YUV420P) {
        srcFmt = AV_PIX_FMT_YUV420P;
    } else if (frame.format == VideoFrame::NV12) {
        srcFmt = AV_PIX_FMT_NV12;
    } else {
        return false;  // Unsupported format for screenshot
    }

    // YUV/NV12 -> RGBA
    std::vector<uint8_t> rgbaData(frame.width * frame.height * 4);

    SwsContext* swsCtx = sws_getContext(
        frame.width, frame.height, srcFmt,
        frame.width, frame.height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!swsCtx) return false;

    const uint8_t* srcSlice[3] = {
        frame.data[0].data(),
        frame.data[1].data(),
        frame.data[2].data()
    };
    int srcStride[3] = { frame.linesize[0], frame.linesize[1], frame.linesize[2] };

    uint8_t* dstSlice[1] = { rgbaData.data() };
    int dstStride[1] = { frame.width * 4 };

    sws_scale(swsCtx, srcSlice, srcStride, 0, frame.height, dstSlice, dstStride);
    sws_freeContext(swsCtx);

    int result = stbi_write_png(savePath.c_str(), frame.width, frame.height,
        4, rgbaData.data(), frame.width * 4);

    return result != 0;
}

void PlayerController::setupAudioCallback() {
    audioOutput_->setCallback([this](uint8_t* output, int size) {
        // Pull data from the audio frame queue to fill the output buffer.
        // Non-blocking: pop(af, 0) returns immediately if no data is available.
        int remaining = size;
        uint8_t* outPtr = output;

        while (remaining > 0) {
            if (audioResidual_.empty()) {
                AudioFrame af;
                if (!audioFrameQueue_.pop(af, 0)) {
                    // No data available — fill rest with silence
                    memset(outPtr, 0, remaining);
                    break;
                }
                audioResidual_ = std::move(af.data);
                audioResidualOffset_ = 0;
            }

            int available = static_cast<int>(audioResidual_.size()) - audioResidualOffset_;
            int toCopy = (std::min)(remaining, available);
            memcpy(outPtr, audioResidual_.data() + audioResidualOffset_, toCopy);
            outPtr += toCopy;
            remaining -= toCopy;
            audioResidualOffset_ += toCopy;

            if (audioResidualOffset_ >= static_cast<int>(audioResidual_.size())) {
                audioResidual_.clear();
                audioResidualOffset_ = 0;
            }
        }
    });
}
