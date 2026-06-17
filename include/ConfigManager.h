/**
 * @file    ConfigManager.h
 * @brief   V2 配网与配置管理模块 — AP 强制门户 + 凭据持久化 + HTTP 重定向
 *
 * @details 本模块是 V2 架构的"入口关卡"模块，负责：
 *          1. 开机从 LittleFS 加载 WiFi 凭据并尝试连接
 *          2. 无凭据/连接失败 → 启动 AP 热点 "OLED-BitStream" + DNS 劫持 + Captive Portal
 *          3. 用户通过极客风格网页输入 SSID/密码 → 持久化到 /wifi.json → 自动重启
 *          4. 连接成功 → 80 端口启动轻量 HTTP 服务，302 重定向至 GitHub Pages 前端控制台
 *
 *          Captive Portal 覆盖主流系统的检测 URL：
 *          Android (generate_204)、Apple (hotspot-detect.html)、Windows (ncsi.txt / fwlink)
 *
 * @warning 本模块在 AP 模式下持有 DNSServer（端口 53）和 ESP8266WebServer（端口 80）；
 *          连接成功后仅保留 ESP8266WebServer（端口 80）用于 302 重定向。
 *          AP 模式设有 5 分钟超时，超时后自动重启以释放资源。
 *
 * @note    凭据文件格式（/wifi.json）：{"s":"MySSID","p":"MyPassword"}
 *          使用 ArduinoJson 进行序列化/反序列化，键名极度压缩以节省 Flash。
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

/**
 * @class ConfigManager
 * @brief V2 核心配网模块 — 自洽的 WiFi 配置与 HTTP 重定向管理
 *
 *        生命周期：
 *        ┌──── begin() ────┐
 *        │  加载凭据        │
 *        │  ↓ 有凭据？      │
 *        │  YES → 连接 WiFi │
 *        │  ↓ 成功？        │
 *        │  YES → 重定向模式│
 *        │  NO  → AP 模式   │
 *        │  ↓ 凭据为空？    │
 *        │  → AP 模式       │
 *        └─────────────────┘
 */
class ConfigManager
{
public:
    /** @brief 模块运行模式 */
    enum class Mode : uint8_t
    {
        INIT,               ///< 初始状态，尚未完成 begin()
        AP_CONFIG,          ///< AP 模式 — 开放热点等待用户配网
        STATION_CONNECTED   ///< 已连接 WiFi — HTTP 重定向服务运行中
    };

    /**
     * @brief 构造 ConfigManager 实例
     *
     * @details 初始化成员变量为安全默认值，不执行任何 I/O 或内存分配。
     */
    ConfigManager();

    /**
     * @brief 模块入口 — 加载凭据 / 连接 WiFi / 启动 AP 或重定向服务
     *
     * @details 完整流程：
     *          1. 挂载 LittleFS 文件系统
     *          2. 尝试读取 /wifi.json 获取保存的 SSID 与密码
     *          3. 若有凭据 → 调用 WiFi.begin() 并等待最多 15 秒
     *          4. 连接成功 → 进入 STATION_CONNECTED 模式，启动 80 端口重定向
     *          5. 连接失败或无凭据 → 进入 AP_CONFIG 模式，开放热点与 Captive Portal
     *
     * @note 本方法在 AP 模式下不会阻塞，立即返回；loop() 中驱动服务。
     */
    void begin();

    /**
     * @brief 驱动内部服务事件循环
     *
     * @details 每帧调用 dnsServer.processNextRequest() 和 httpServer.handleClient()。
     *          在 AP 模式下还检查超时（5 分钟无配网自动重启）。
     *          调用方应在 Arduino loop() 中高频调用（≥100Hz）。
     */
    void loop();

    /**
     * @brief 获取当前运行模式
     *
     * @return Mode 枚举值，用于上层模块决策（如 OLED 显示不同状态页）
     */
    Mode getMode() const;

    /**
     * @brief 检查 WiFi 是否已成功连接
     *
     * @return true  WiFi 已连接且获得 IP
     * @return false 未连接或处于 AP 模式
     */
    bool isWiFiConnected() const;

private:
    /* ====================================================================== */
    /*  凭据持久化                                                            */
    /* ====================================================================== */

    /**
     * @brief 从 LittleFS 读取保存的 WiFi 凭据
     *
     * @param[out] ssid     读取到的 SSID
     * @param[out] password 读取到的密码
     * @return true  凭据读取成功
     * @return false 文件不存在或 JSON 解析失败
     */
    bool loadCredentials(String& ssid, String& password);

    /**
     * @brief 将 WiFi 凭据保存至 LittleFS
     *
     * @param ssid     要保存的 SSID
     * @param password 要保存的密码
     * @return true  写入成功
     * @return false 文件系统写入失败
     */
    bool saveCredentials(const String& ssid, const String& password);

    /* ====================================================================== */
    /*  WiFi 连接                                                             */
    /* ====================================================================== */

    /**
     * @brief 尝试连接指定 WiFi 网络（带超时）
     *
     * @param ssid       目标 SSID
     * @param password   目标密码
     * @param timeoutSec 超时秒数（默认 15 秒）
     * @return true  连接成功并获取 IP
     * @return false 超时或连接失败
     */
    bool connectWiFi(const String& ssid, const String& password, uint16_t timeoutSec = 15);

    /* ====================================================================== */
    /*  AP 模式                                                               */
    /* ====================================================================== */

    /**
     * @brief 启动 AP 热点与 Captive Portal
     *
     * @details 1. 配置 SoftAP（SSID="OLED-BitStream"，开放无密码）
     *          2. 启动 DNS 服务器，将所有域名劫持至 192.168.4.1
     *          3. 注册 HTTP 路由（配置页 + 提交接口 + 通配捕获）
     *          4. 记录 AP 启动时间戳用于超时检测
     */
    void startAPMode();

    /**
     * @brief 处理 Captive Portal 的所有 HTTP 请求
     *
     * @details 区分三类请求：
     *          - GET / → 返回配网页（极客终端风格 HTML）
     *          - POST /save → 解析表单参数，保存凭据，返回成功页并重启
     *          - 其他路径 → 302 重定向至 /（覆盖所有 Captive Portal 检测 URL）
     */
    void handleCaptiveRequest();

    /**
     * @brief 返回配网页 HTML（内嵌 PROGMEM 字符串）
     */
    void serveConfigPage();

    /**
     * @brief 处理配网页提交的表单数据
     */
    void handleSaveConfig();

    /* ====================================================================== */
    /*  Station 已连接模式                                                     */
    /* ====================================================================== */

    /**
     * @brief 切换到已连接模式，启动 HTTP 重定向服务
     *
     * @details 配置 ESP8266WebServer 的 80 端口：
     *          - GET / → 302 重定向至 GitHub Pages 前端（附带 ?ip= 参数）
     *          - 其他路径 → 同样返回 302（避免 404）
     *          同时关闭 DNS 服务器（不再需要劫持）。
     */
    void startStationMode();

    /* ====================================================================== */
    /*  成员变量                                                              */
    /* ====================================================================== */

    Mode            m_mode;             ///< 当前运行模式
    DNSServer       m_dnsServer;        ///< DNS 劫持服务器（端口 53）
    ESP8266WebServer m_httpServer;      ///< HTTP 服务器（端口 80）
    unsigned long   m_apStartTime;      ///< AP 模式启动时间戳（毫秒）

    /* ====================================================================== */
    /*  常量                                                                  */
    /* ====================================================================== */

    static const char*  CONFIG_FILE;    ///< 凭据文件路径 "/wifi.json"
    static const char*  AP_SSID;        ///< AP 热点名称 "OLED-BitStream"
    static const IPAddress AP_IP;       ///< AP 模式网关/ DNS IP
    static const IPAddress AP_SUBNET;   ///< AP 模式子网掩码
    static constexpr unsigned long AP_TIMEOUT_MS = 300000;  ///< AP 超时 5 分钟
    static constexpr uint16_t WIFI_CONNECT_TIMEOUT_SEC = 15; ///< WiFi 连接超时
    static constexpr uint16_t HTTP_PORT = 80;                ///< HTTP 服务端口
    static constexpr uint16_t DNS_PORT = 53;                 ///< DNS 劫持端口
    static const char*  REDIRECT_BASE_URL; ///< GitHub Pages 前端 URL
};

#endif // CONFIG_MANAGER_H
