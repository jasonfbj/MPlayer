#pragma once

#include "core/common/NonCopyable.h"
#include "core/common/VideoFrame.h"
#include "core/common/BlockingQueue.h"
#include "core/audio/AudioFrame.h"
#include "core/demuxer/Demuxer.h"
#include "core/demuxer/NetworkConfig.h"
#include "core/decoder/IDecoder.h"
#include "core/decoder/DecoderFactory.h"
#include "core/renderer/IRenderer.h"
#include "core/audio/IAudioOutput.h"
#include "core/audio/AudioResampler.h"

#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <functional>
#include <mutex>

struct VideoInfo {
    std::string codecName;
    int width = 0;
    int height = 0;
    double duration = 0;
    int bitrate = 0;
    double frameRate = 0;
};

class PlayerController : public NonCopyable {
public:
    PlayerController();
    ~PlayerController();

    bool open(const std::string& url);
    bool open(const std::string& url, const NetworkConfig& config);
    void close();

    void play();
    void pause();
    void stop();
    void seek(double seconds);
    void setSpeed(float speed);
    void setVolume(float volume);

    enum State { Idle, Playing, Paused, Stopped };
    State state() const { return state_; }
    double duration() const;
    double currentPosition() const;

    void setRenderer(std::unique_ptr<IRenderer> renderer);
    void setAudioOutput(std::unique_ptr<IAudioOutput> audioOutput);

    using FrameCallback = std::function<void(const VideoFrame&)>;
    void setVideoFrameCallback(FrameCallback cb) { videoFrameCb_ = std::move(cb); }

    using StateCallback = std::function<void(State)>;
    void setStateCallback(StateCallback cb) { stateCb_ = std::move(cb); }

    using ErrorCallback = std::function<void(const std::string&)>;
    void setErrorCallback(ErrorCallback cb) { errorCb_ = std::move(cb); }

    using ConnectionCallback = std::function<void(ConnectionState, const std::string&)>;
    void setConnectionCallback(ConnectionCallback cb);
    ConnectionState connectionState() const;

    VideoInfo getVideoInfo() const;

    bool captureFrame(const std::string& savePath);

    // 获取帧队列供外部消费
    BlockingQueue<VideoFrame>& videoFrameQueue() { return videoFrameQueue_; }
    BlockingQueue<AudioFrame>& audioFrameQueue() { return audioFrameQueue_; }

    // 播放完成查询（EOF 且所有帧已消费）
    bool isPlaybackComplete() const;
    bool isEof() const { return eof_.load(); }

private:
    void readThread();
    void videoDecodeThread();
    void audioDecodeThread();

    void setState(State s);
    void initAudioResampler();
    void setupAudioCallback();

    std::unique_ptr<Demuxer> demuxer_;
    std::unique_ptr<IDecoder> videoDecoder_;
    std::unique_ptr<IDecoder> audioDecoder_;
    std::unique_ptr<IRenderer> renderer_;
    std::unique_ptr<IAudioOutput> audioOutput_;
    std::unique_ptr<AudioResampler> audioResampler_;

    BlockingQueue<AVPacket*> videoPacketQueue_;
    BlockingQueue<AVPacket*> audioPacketQueue_;
    BlockingQueue<VideoFrame> videoFrameQueue_;
    BlockingQueue<AudioFrame> audioFrameQueue_;

    std::thread readThread_;
    std::thread videoDecodeThread_;
    std::thread audioDecodeThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> eof_{false};

    // Pause/seek synchronization: decode threads wait on this when paused
    std::mutex pauseMutex_;
    std::condition_variable pauseCond_;
    std::atomic<bool> paused_{false};

    // Seek request: readThread performs the seek on its own thread,
    // avoiding main-thread blocking on network I/O
    std::atomic<bool> seekRequested_{false};
    std::atomic<double> seekTarget_{0.0};

    // When set, decode threads will pause after processing the seek target
    std::atomic<bool> seekPauseAfterDecode_{false};

    // Flush request flags: set by seek(), handled by decode threads themselves
    // to avoid concurrent AVCodecContext access (not thread-safe in FFmpeg)
    std::atomic<bool> videoFlushRequested_{false};
    std::atomic<bool> audioFlushRequested_{false};

    std::atomic<State> state_{Idle};
    std::atomic<float> speed_{1.0f};
    std::atomic<double> currentPosition_{0.0};
    std::string currentUrl_;
    mutable std::mutex mutex_;

    mutable std::mutex frameMutex_;
    VideoFrame lastFrame_;

    FrameCallback videoFrameCb_;
    StateCallback stateCb_;
    ErrorCallback errorCb_;

    // Audio residual buffer for feeding partial frames to audio output
    std::vector<uint8_t> audioResidual_;
    int audioResidualOffset_ = 0;
};
