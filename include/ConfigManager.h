/**
 * @file    ConfigManager.h
 * @brief   V2 配网与配置管理模块 — AP 强制门户 + 凭据持久化 + HTTP 重定向
 *
 * @details 本模块是 V2 架构的"入口关卡"模块，负责：
 *          1. 开机从 LittleFS 加载 WiFi 凭据并尝试连接
 *          2. 无凭据/连接失败 → 启动 AP 热点 "OLED-BitStream" + DNS 劫持 + Captive Portal
 *          3. 用户通过极客风格网页输入 SSID/密码 → 持久化到 /wifi.json → 自动重启
 *          4. 连接成功 → 80 端口启动轻量 HTTP 服务，302 重定向至 GitHub Pages 前端控制台
 *          5. AP 模式下通过 StatusCallback 实时向 OLED 推送连接状态
 *
 *          Captive Portal 针对全平台优化：
 *          - 对已知检测 URL 直接返回 200 OK（非 302 重定向），提升兼容性
 *          - 对 ncsi.txt/connecttest.txt 返回 200 + "Microsoft Connect Test"
 *          - 对 generate_204 返回 204 No Content
 *          - DNS 劫持覆盖 15+ 个常见检测域名
 *
 * @warning 本模块在 AP 模式下持有 DNSServer（端口 53）和 ESP8266WebServer（端口 80）；
 *          连接成功后仅保留 ESP8266WebServer（端口 80）用于 302 重定向。
 *          AP 模式设有 5 分钟超时，超时后自动重启以释放资源。
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <functional>

class ConfigManager
{
public:
    /* ====================================================================== */
    /*  类型定义                                                               */
    /* ====================================================================== */

    /** @brief 模块运行模式 */
    enum class Mode : uint8_t
    {
        INIT,               ///< 初始状态，尚未完成 begin()
        AP_CONFIG,          ///< AP 模式 — 开放热点等待用户配网
        STATION_CONNECTED   ///< 已连接 WiFi — HTTP 重定向服务运行中
    };

    /**
     * @brief 状态更新回调（用于 OLED 实时反馈）
     * @param message 要显示的状态文本（C 字符串，调用方应立即消费）
     */
    using StatusCallback = std::function<void(const char *message)>;

    /* ====================================================================== */
    /*  构造函数                                                              */
    /* ====================================================================== */

    ConfigManager();

    /* ====================================================================== */
    /*  公有接口                                                              */
    /* ====================================================================== */

    /**
     * @brief 模块入口 — 加载凭据 / 连接 WiFi / 启动 AP 或重定向服务
     *
     * @param onStatus 可选的状态回调，用于 OLED 实时反馈
     *                 （如 "Connecting...", "Client Connected!", 等）
     */
    void begin(StatusCallback onStatus = nullptr);

    /** @brief 驱动内部服务事件循环 */
    void loop();

    /** @brief 获取当前运行模式 */
    Mode getMode() const;

    /** @brief 检查 WiFi 是否已成功连接 */
    bool isWiFiConnected() const;

private:
    /* ====================================================================== */
    /*  凭据持久化                                                            */
    /* ====================================================================== */
    bool loadCredentials(String &ssid, String &password);
    bool saveCredentials(const String &ssid, const String &password);

    /* ====================================================================== */
    /*  WiFi 连接                                                             */
    /* ====================================================================== */
    bool connectWiFi(const String &ssid, const String &password, uint16_t timeoutSec = 15);

    /* ====================================================================== */
    /*  AP 模式                                                               */
    /* ====================================================================== */
    void startAPMode();
    void serveConfigPage();
    void handleSaveConfig();

    /** @brief 直接返回配网页（200 OK），用于 Captive Portal 检测 URL */
    void serveCaptivePage();

    /** @brief 返回 204 No Content（Android generate_204 检测） */
    void serve204NoContent();

    /** @brief 返回 Windows 连接测试文本 */
    void serveNcsiOK();



    /* ====================================================================== */
    /*  Station 已连接模式                                                     */
    /* ====================================================================== */
    void startStationMode();

    /* ====================================================================== */
    /*  成员变量                                                              */
    /* ====================================================================== */

    Mode             m_mode;
    DNSServer        m_dnsServer;
    ESP8266WebServer m_httpServer;
    unsigned long    m_apStartTime;
    StatusCallback   m_statusCallback;

    /// WiFi 事件处理器 — 监听 SoftAP 客户端连接/断开
    WiFiEventHandler m_onStationConnected;
    WiFiEventHandler m_onStationDisconnected;

    /* ====================================================================== */
    /*  常量                                                                  */
    /* ====================================================================== */
    static const char *CONFIG_FILE;
    static const char *AP_SSID;
    static const IPAddress AP_IP;
    static const IPAddress AP_SUBNET;
    static constexpr unsigned long AP_TIMEOUT_MS = 300000;
    static constexpr uint16_t WIFI_CONNECT_TIMEOUT_SEC = 15;
    static constexpr uint16_t HTTP_PORT = 80;
    static constexpr uint16_t DNS_PORT  = 53;
    static const char *REDIRECT_BASE_URL;

    /**
     * @brief 需要 DNS 劫持的已知 Captive Portal 检测域名
     *
     * @details Android / Apple / Windows / Firefox 各平台会在连接热点后
     *          向这些域名发起 HTTP 请求以检测是否存在强制门户。
     *          我们在 DNS 层将所有域名解析至 192.168.4.1，并在 HTTP 层
     *          对已知检测 URL 返回 200 OK（非 302）以最大化兼容性。
     */
    
    
};

#endif // CONFIG_MANAGER_H
