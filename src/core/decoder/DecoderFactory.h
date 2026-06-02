#pragma once

#include "core/decoder/IDecoder.h"
#include <memory>
#include <string>
#include <functional>

class DecoderFactory {
public:
    enum class DecoderType {
        Auto,
        Software,
        Hardware
    };

    // sharedDevice: Windows=ID3D11Device*, Android=jobject Surface (cast to void*)
    using HardwareCreator = std::function<std::unique_ptr<IDecoder>(void* sharedDevice)>;

    static std::unique_ptr<IDecoder> createVideoDecoder(
        const AVCodecParameters* params,
        DecoderType type = DecoderType::Auto,
        void* sharedDevice = nullptr
    );

    static std::unique_ptr<IDecoder> createAudioDecoder(
        const AVCodecParameters* params
    );

    static bool isHardwareDecodeAvailable(const AVCodecParameters* params);

    static void registerHardwareCreator(HardwareCreator creator);

private:
    static HardwareCreator& getHardwareCreator();
};
