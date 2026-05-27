#pragma once

#include "core/renderer/IRenderer.h"
#include <memory>
#include <functional>
#include <string>

class RendererFactory {
public:
    using Creator = std::function<std::unique_ptr<IRenderer>()>;

    static void registerRenderer(const std::string& platform, Creator creator);
    static std::unique_ptr<IRenderer> create(const std::string& platform);

private:
    static Creator& getCreator();
};
