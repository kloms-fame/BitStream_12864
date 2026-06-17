/**
 * @file    WebServerManager.h
 * @brief   V2 Web 服务器管理 — DNS 劫持 + 动态路由 + 强制门户
 *
 * @details 本模块负责 HTTP (80 端口) 和 DNS (53 端口) 的所有逻辑。
 *          根据 NetworkManager::isAPMode() 动态切换行为：
 *
 *          AP 模式 (离线):
 *          - DNS 劫持 * → 192.168.4.1
 *          - Captive Portal 精准拦截 (generate_204 等)
 *          - GET / → LittleFS 提供 /offline.html
 *          - POST /api/setwifi → NetworkManager::saveWiFiAndReboot()
 *
 *          STA 模式 (在线):
 *          - GET / → HTTP 302 → GitHub Pages 控制台
 *          - DNS 不启动
 *
 *          零网络管理依赖：仅通过 NetworkManager 静态方法查询/写入，
 *          不持有 NetworkManager 实例指针。
 */

#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

class WebServerManager
{
public:
    /* ================================================================== */
    /*  常量                                                               */
    /* ================================================================== */

    static constexpr uint16_t HTTP_PORT = 80;
    static constexpr uint16_t DNS_PORT  = 53;

    /* ================================================================== */
    /*  构造函数 / 公有接口                                                 */
    /* ================================================================== */

    WebServerManager();

    /**
     * @brief 注册全部路由并启动 HTTP 服务（AP 模式额外启动 DNS 劫持）
     *
     * @details 路由表：
     *          GET  /                       → serveRoot() (AP→离线页 / STA→302)
     *          POST /api/setwifi            → handleSetWiFi()
     *          GET  /generate_204           → 302 → /
     *          GET  /hotspot-detect.html    → 302 → /
     *          GET  /library/test/success.html → 302 → /
     *          GET  /canonical.html         → 302 → /
     *          GET  /success.txt            → 302 → /
     *          GET  /ncsi.txt               → "Microsoft Connect Test"
     *          GET  /connecttest.txt        → "Microsoft Connect Test"
     *          GET  /redirect               → 302 → /
     *          GET  /fwlink                 → 302 → /
     *          GET  /check_network_status.txt → 302 → /
     *          其他所有路径                  → 302 → /
     */
    void begin();

    /** @brief 驱动 DNS + HTTP 事件循环 */
    void loop();

private:
    /* ================================================================== */
    /*  路由处理器                                                         */
    /* ================================================================== */

    void serveRoot();
    void handleSetWiFi();
    void serveOfflinePage();
    void redirectToRoot();
    void serveNcsiOK();
    void serve204NoContent();

    /* ================================================================== */
    /*  成员变量                                                           */
    /* ================================================================== */

    DNSServer        m_dnsServer;
    ESP8266WebServer m_httpServer;
};

#endif // WEB_SERVER_MANAGER_H
