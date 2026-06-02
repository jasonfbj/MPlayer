#pragma once

#include <functional>
#include <string>

struct NetworkConfig {
    int timeoutUs = 5000000;        // 网络超时 (微秒), 默认 5 秒
    int bufferSize = 1024000;       // 缓冲区大小 (字节)
    int maxDelay = 500000;          // 最大延迟 (微秒)
    int maxRetries = 3;             // 最大重连次数
    int retryBaseDelayMs = 1000;    // 重连基础间隔 (毫秒)
};

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Failed
};

using ConnectionCallback = std::function<void(ConnectionState, const std::string&)>;
