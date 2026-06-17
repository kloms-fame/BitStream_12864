/**
 * @file    NetworkManager.cpp
 * @brief   NetworkManager 类的方法实现
 *
 * @details 完整实现 WebSocket 服务端（端口 81）：
 *          - 二进制帧（1024B）接收 → 回调触发 → ACK 回复
 *          - 文本指令 "BRIGHTNESS:XXX" 解析 → 回调触发 → OK 回复
 *          - 客户端连接/断开日志与统计
 *          - 零显示依赖、零 WiFi 管理依赖
 */

#include "NetworkManager.h"

/* ========================================================================== */
/*  构造函数                                                                  */
/* ========================================================================== */

NetworkManager::NetworkManager()
    : m_webSocket(WS_PORT)
{
}

/* ========================================================================== */
/*  begin() — 启动 WebSocket 服务器并注册双回调                                */
/* ========================================================================== */

void NetworkManager::begin(FrameCallback onFrame, BrightnessCallback onBrightness)
{
    m_webSocket.begin();
    Serial.printf("[NET] WebSocket 服务器已启动 — 端口 %u\n", WS_PORT);

    // ── 注册 WebSocket 事件处理器 ──────────────────────────────────────
    m_webSocket.onEvent(
        [this, onFrame = std::move(onFrame), onBrightness = std::move(onBrightness)]
        (uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length)
        {
            switch (type)
            {
            /* ---------------------------------------------------------- */
            /*  客户端连接                                                   */
            /* ---------------------------------------------------------- */
            case WStype_CONNECTED:
            {
                const IPAddress remoteIP = m_webSocket.remoteIP(clientNum);
                Serial.printf("[NET] 客户端 #%u 已连接 — IP: %s\n",
                              clientNum,
                              remoteIP.toString().c_str());
                break;
            }

            /* ---------------------------------------------------------- */
            /*  客户端断开                                                   */
            /* ---------------------------------------------------------- */
            case WStype_DISCONNECTED:
            {
                Serial.printf("[NET] 客户端 #%u 已断开\n", clientNum);
                break;
            }

            /* ---------------------------------------------------------- */
            /*  二进制帧 — 推流数据入口                                        */
            /* ---------------------------------------------------------- */
            case WStype_BIN:
            {
                if (length == FRAME_SIZE)
                {
                    // 1. 调用上层渲染回调（零拷贝传递指针）
                    //    回调内部应立即消费 payload 数据（例如 memcpy 到帧缓冲）
                    if (onFrame)
                    {
                        onFrame(payload, length);
                    }

                    // 2. 发送应用层 ACK 确认
                    //    前端收到 "ACK" 后才推送下一帧，实现锁步背压控制
                    //    这从根本上杜绝了 TCP 发送缓冲区无限堆积的问题
                    m_webSocket.sendTXT(clientNum, "ACK");
                }
                else
                {
                    // 异常帧长度：记录日志但不中断服务
                    Serial.printf("[NET] 客户端 #%u 发送异常 BIN 帧: "
                                  "期望 %u 字节，实际 %u 字节\n",
                                  clientNum, FRAME_SIZE, length);
                }
                break;
            }

            /* ---------------------------------------------------------- */
            /*  文本帧 — 控制指令入口                                          */
            /* ---------------------------------------------------------- */
            case WStype_TEXT:
            {
                // payload 是 C 字符串，可直接解析
                const char *text = reinterpret_cast<const char *>(payload);

                // 尝试解析亮度控制指令 "BRIGHTNESS:XXX"
                uint8_t brightness = 0;
                if (parseBrightnessCommand(text, brightness))
                {
                    Serial.printf("[NET] 客户端 #%u 指令: 亮度 → %u\n",
                                  clientNum, brightness);

                    // 调用亮度回调（→ DisplayManager::setBrightness）
                    if (onBrightness)
                    {
                        onBrightness(brightness);
                    }

                    // 回复确认
                    m_webSocket.sendTXT(clientNum, "OK");
                }
                else
                {
                    // 未知文本指令：记录后静默忽略
                    Serial.printf("[NET] 客户端 #%u 发送未知指令: \"%s\"\n",
                                  clientNum, text);
                }
                break;
            }

            /* ---------------------------------------------------------- */
            /*  PING / PONG / ERROR / 其他事件 — 静默忽略                     */
            /* ---------------------------------------------------------- */
            default:
                break;
            }
        });
}

/* ========================================================================== */
/*  loop() — WebSocket 事件循环                                               */
/* ========================================================================== */

void NetworkManager::loop()
{
    m_webSocket.loop();
}

/* ========================================================================== */
/*  getClientCount()                                                           */
/* ========================================================================== */

uint8_t NetworkManager::getClientCount()
{
    return m_webSocket.connectedClients();
}

/* ========================================================================== */
/*  parseBrightnessCommand() — 静态指令解析器                                   */
/* ========================================================================== */

bool NetworkManager::parseBrightnessCommand(const char *text, uint8_t &brightness)
{
    // 匹配前缀 "BRIGHTNESS:"（大小写不敏感）
    // 格式：BRIGHTNESS:0 至 BRIGHTNESS:255
    // 示例："BRIGHTNESS:128" → brightness = 128

    const char prefix[] = "BRIGHTNESS:";
    constexpr size_t prefixLen = sizeof(prefix) - 1; // 不含 '\0'

    // 大小写不敏感前缀匹配
    for (size_t i = 0; i < prefixLen; ++i)
    {
        char c = text[i];
        if (c == '\0') return false;           // 文本短于前缀
        if (c >= 'a' && c <= 'z') c -= 32;     // 转大写
        if (c != prefix[i]) return false;       // 前缀不匹配
    }

    // 解析冒号后的数字部分
    const char *numStr = text + prefixLen;
    if (*numStr == '\0') return false;         // 缺少数值

    // 手动 atoi（避免 String 堆分配，关键路径优化）
    uint16_t value = 0;
    for (const char *p = numStr; *p != '\0'; ++p)
    {
        if (*p < '0' || *p > '9') return false; // 含非数字字符
        value = value * 10 + (*p - '0');
        if (value > 255) return false;          // 数值越界
    }

    brightness = static_cast<uint8_t>(value);
    return true;
}
