#pragma once

#include <string>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class Demuxer {
public:
    Demuxer();
    ~Demuxer();

    bool open(const std::string& url);
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

private:
    AVFormatContext* formatCtx_ = nullptr;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    std::atomic<bool> opened_{false};
    std::atomic<bool> eof_{false};
};
