/**
 * @file    NetworkManager.h
 * @brief   V2 网络内核 — AP/STA 双状态机 + WebSocket 流媒体 + 凭据持久化
 *
 * @details 本模块是 V2 架构唯一的"网络层"，统一管理 WiFi STA/AP、WebSocket 推流、凭据持久化
 *          WebServerManager 的冗余设计，实现高内聚低耦合：
 *
 *          启动序列：
 *          begin() → LittleFS 加载 /wifi.txt → STA 3次重试 → 失败则 AP
 *
 *          ┌─────────────────────────────────────────────┐
 *          │              NetworkManager                  │
 *          │  WiFi STA/AP  │  WebSocket:81  │  凭据 I/O  │
 *          └──────┬────────┴───────▲────────┴─────┬──────┘
 *                 │                │               │
 *          静态查询接口       帧/亮度回调      saveWiFiAndReboot
 *          isAPMode()      DisplayManager      WebServerManager
 *
 *          零外部依赖耦合：
 *          - 不包含任何 OLED/U8g2/HTTP/DNS 代码
 *          - 仅通过 std::function 回调与 DisplayManager 通信
 *          - 仅通过静态方法向 WebServerManager 暴露状态和写入接口
 *
 * @warning begin() 在 STA 模式下会阻塞最多 45 秒（3次 × 15秒超时），
 *          此期间不处理任何事件循环。
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <functional>

class NetworkManager
{
public:
    /* ================================================================== */
    /*  类型定义                                                           */
    /* ================================================================== */

    /** @brief 网络运行模式 */
    enum class Mode : uint8_t
    {
        INIT,
        AP_CONFIG,
        STATION_CONNECTED
    };

    using FrameCallback = std::function<void(uint8_t *payload, size_t length)>;
    using BrightnessCallback = std::function<void(uint8_t brightness)>;

    /* ================================================================== */
    /*  常量                                                               */
    /* ================================================================== */

    static constexpr uint16_t WS_PORT       = 81;
    static constexpr size_t   FRAME_SIZE    = 1024;
    static constexpr uint8_t  BRIGHTNESS_MIN = 0;
    static constexpr uint8_t  BRIGHTNESS_MAX = 255;
    static constexpr uint8_t  MAX_RETRIES    = 3;

    /* ================================================================== */
    /*  构造函数                                                           */
    /* ================================================================== */

    NetworkManager();

    /* ================================================================== */
    /*  公有接口                                                           */
    /* ================================================================== */

    void begin(FrameCallback onFrame, BrightnessCallback onBrightness);
    void loop();
    static bool isAPMode();
    static void saveWiFiAndReboot(const char *ssid, const char *pass);
    uint8_t getClientCount();

private:
    /* ================================================================== */
    /*  内部方法                                                           */
    /* ================================================================== */

    bool loadCredentials(String &ssid, String &pass);
    bool connectWiFi(const char *ssid, const char *pass);
    void startAPMode();
    void startWebSocket();
    static bool parseBrightnessCommand(const char *text, uint8_t &brightness);

    /* ================================================================== */
    /*  成员变量                                                           */
    /* ================================================================== */

    static Mode        s_mode;
    WebSocketsServer   m_webSocket;
    FrameCallback      m_onFrame;
    BrightnessCallback m_onBrightness;

    /* ================================================================== */
    /*  常量                                                               */
    /* ================================================================== */

    static const char *CRED_FILE;
    static const char *AP_SSID;
};

#endif // NETWORK_MANAGER_H
