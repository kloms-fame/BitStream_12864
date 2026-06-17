/**
 * @file    NetworkManager.h
 * @brief   V2 通信内核 — WebSocket 二进制流接收与指令解析
 *
 * @details 本模块是 V2 架构的"传输层"，严格遵循高内聚低耦合原则：
 *          - 零显示依赖：不包含任何 OLED/U8g2/像素操作代码
 *          - 零 WiFi 管理：WiFi 连接由 ConfigManager 模块独立负责
 *          - 仅负责 WebSocket 帧的接收、分发与 ACK 反馈
 *
 *          双回调架构：
 *          FrameCallback       — 收到 1024 字节二进制帧时调用（→ DisplayManager::renderFrame）
 *          BrightnessCallback  — 收到 "BRIGHTNESS:XXX" 指令时调用（→ DisplayManager::setBrightness）
 *
 *          应用层 ACK 锁步机制：
 *          每帧渲染完成后立即发送 sendTXT("ACK")，前端收到 ACK 后才推送下一帧，
 *          从而在应用层实现背压控制，彻底杜绝 TCP 发送缓冲区堆积导致的延迟雪崩。
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <functional>

/**
 * @class NetworkManager
 * @brief WebSocket 服务端（端口 81）— 二进制帧接收 + 文本指令解析
 *
 *        通信协议：
 *        ┌──────────────────────────────────────────────┐
 *        │  方向        类型      内容                   │
 *        │  PC → ESP    BIN       1024 字节 XBM 位图帧  │
 *        │  ESP → PC    TXT       "ACK"（帧确认）       │
 *        │  PC → ESP    TXT       "BRIGHTNESS:0~255"    │
 *        │  ESP → PC    TXT       "OK"（指令确认）       │
 *        └──────────────────────────────────────────────┘
 */
class NetworkManager
{
public:
    /* ====================================================================== */
    /*  回调类型定义                                                           */
    /* ====================================================================== */

    /**
     * @brief 帧渲染回调
     * @param payload 指向 1024 字节二进制位图数据的指针
     * @param length  数据长度（恒为 1024）
     */
    using FrameCallback = std::function<void(uint8_t *payload, size_t length)>;

    /**
     * @brief 亮度控制回调
     * @param brightness 亮度值 [0, 255]
     */
    using BrightnessCallback = std::function<void(uint8_t brightness)>;

    /* ====================================================================== */
    /*  常量                                                                  */
    /* ====================================================================== */

    static constexpr uint16_t WS_PORT      = 81;   ///< WebSocket 服务端口
    static constexpr size_t   FRAME_SIZE   = 1024; ///< 标准帧字节数 (128×64÷8)
    static constexpr uint8_t  BRIGHTNESS_MIN = 0;
    static constexpr uint8_t  BRIGHTNESS_MAX = 255;

    /* ====================================================================== */
    /*  公有接口                                                              */
    /* ====================================================================== */

    /**
     * @brief 构造 NetworkManager 实例
     *
     * @details 在初始化列表中创建监听 81 端口的 WebSocket 服务端。
     *          构造阶段不启动服务（由 begin() 启动）。
     */
    NetworkManager();

    /**
     * @brief 启动 WebSocket 服务器并注册双回调
     *
     * @param onFrame      帧渲染回调（收到 1024B 二进制帧时调用）
     * @param onBrightness 亮度控制回调（收到 BRIGHTNESS 指令时调用）
     *
     * @details 注册 WebSocket 事件处理器，涵盖：
     *          - 客户端连接/断开日志
     *          - WStype_BIN：校验 1024 字节长度 → 调用 onFrame → 发送 ACK
     *          - WStype_TEXT：解析 "BRIGHTNESS:XXX" → 调用 onBrightness → 发送 OK
     *          - 其他事件静默忽略
     */
    void begin(FrameCallback onFrame, BrightnessCallback onBrightness);

    /**
     * @brief 驱动 WebSocket 内部事件循环
     *
     * @details 转发至 webSocket.loop()，需在 Arduino loop() 中高频调用。
     *          此方法是 WebSocket 帧到达的唯一切入点。
     */
    void loop();

    /**
     * @brief 获取当前连接的客户端数量
     *
     * @return 已连接的 WebSocket 客户端数
     */
    uint8_t getClientCount();

private:
    /* ====================================================================== */
    /*  指令解析                                                              */
    /* ====================================================================== */

    /**
     * @brief 尝试解析文本指令为亮度值
     *
     * @param text  原始文本字符串
     * @param[out] brightness 解析出的亮度值 [0, 255]
     * @return true  成功解析 "BRIGHTNESS:XXX" 格式
     * @return false 格式不匹配或数值越界
     */
    static bool parseBrightnessCommand(const char *text, uint8_t &brightness);

    /* ====================================================================== */
    /*  成员变量                                                              */
    /* ====================================================================== */

    WebSocketsServer m_webSocket; ///< 监听 81 端口的 WebSocket 服务端实例
};

#endif // NETWORK_MANAGER_H
