#include "core/decoder/DecoderFactory.h"
#include "core/decoder/SoftwareDecoder.h"

DecoderFactory::HardwareCreator& DecoderFactory::getHardwareCreator() {
    static HardwareCreator creator;
    return creator;
}

void DecoderFactory::registerHardwareCreator(HardwareCreator creator) {
    getHardwareCreator() = std::move(creator);
}

std::unique_ptr<IDecoder> DecoderFactory::createVideoDecoder(
    const AVCodecParameters* params,
    DecoderType type
) {
    if (type == DecoderType::Hardware || type == DecoderType::Auto) {
        auto& hwCreator = getHardwareCreator();
        if (hwCreator) {
            auto decoder = hwCreator();
            if (decoder && decoder->init(params)) {
                return decoder;
            }
        }
    }

    auto decoder = std::make_unique<SoftwareDecoder>();
    if (decoder->init(params)) {
        return decoder;
    }
    return nullptr;
}

std::unique_ptr<IDecoder> DecoderFactory::createAudioDecoder(
    const AVCodecParameters* params
) {
    auto decoder = std::make_unique<SoftwareDecoder>();
    if (decoder->init(params)) {
        return decoder;
    }
    return nullptr;
}

bool DecoderFactory::isHardwareDecodeAvailable(const AVCodecParameters* params) {
    auto& hwCreator = getHardwareCreator();
    return static_cast<bool>(hwCreator);
}
