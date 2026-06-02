#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <chrono>

#include "core/demuxer/NetworkConfig.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class Demuxer {
public:
    Demuxer();
    ~Demuxer();

    bool open(const std::string& url);
    bool open(const std::string& url, const NetworkConfig& config);
    void close();

    bool readPacket(AVPacket* packet);

    int getVideoStreamIndex() const { return videoStreamIndex_; }
    int getAudioStreamIndex() const { return audioStreamIndex_; }
    AVCodecParameters* getVideoParams() const;
    AVCodecParameters* getAudioParams() const;
    AVFormatContext* getFormatContext() const { return formatCtx_; }

    bool seek(double seconds);
    bool isStream() const;
    bool isOpened() const { return opened_; }

    double duration() const;

    void setConnectionCallback(ConnectionCallback cb);
    ConnectionState connectionState() const;

private:
    AVFormatContext* formatCtx_ = nullptr;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    std::atomic<bool> opened_{false};
    std::atomic<bool> eof_{false};

    NetworkConfig netConfig_;
    ConnectionCallback connCb_;
    std::atomic<ConnectionState> connState_{ConnectionState::Disconnected};
    std::string currentUrl_;

    bool reconnectAndRead(AVPacket* packet);

    void setConnState(ConnectionState s, const std::string& msg = "") {
        connState_ = s;
        if (connCb_) connCb_(s, msg);
    }
};
