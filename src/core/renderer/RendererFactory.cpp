#include "core/renderer/RendererFactory.h"

RendererFactory::Creator& RendererFactory::getCreator() {
    static Creator creator;
    return creator;
}

void RendererFactory::registerRenderer(const std::string& platform, Creator creator) {
    getCreator() = std::move(creator);
}

std::unique_ptr<IRenderer> RendererFactory::create(const std::string& platform) {
    auto& creator = getCreator();
    if (creator) {
        return creator();
    }
    return nullptr;
}
