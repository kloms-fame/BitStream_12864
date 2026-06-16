/**
 * @file    WebServerManager.h
 * @brief   Web 服务器管理模块 — 托管 LittleFS 中的静态页面
 *
 * @details 监听 80 端口，收到 GET / 请求时从 LittleFS 读取
 *          index.html 并以 text/html 类型返回。
 *          与 NetworkManager（WebSocket 81 端口）互不干扰。
 */

#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <ESP8266WebServer.h>
#include <LittleFS.h>

/**
 * @class WebServerManager
 * @brief 管理 ESP8266 HTTP 服务器（端口 80），从 LittleFS 托管前端页面
 */
class WebServerManager
{
public:
    /**
     * @brief 构造 WebServerManager 实例
     *
     * @details 初始化列表创建 80 端口 HTTP 服务端对象。
     */
    WebServerManager();

    /**
     * @brief 挂载 LittleFS 并注册路由
     *
     * @details 初始化 LittleFS 文件系统，注册 GET / 路由：
     *          读取 /index.html 并以 text/html 返回。
     *          随后调用 server.begin() 启动 HTTP 服务。
     */
    void begin();

    /**
     * @brief 驱动 HTTP 服务器事件循环
     *
     * @details 内部仅调用 server.handleClient()，
     *          需在 main loop() 中高频调用。
     */
    void loop();

private:
    ESP8266WebServer server; ///< 监听 80 端口的 HTTP 服务器实例
};

#endif // WEB_SERVER_MANAGER_H
