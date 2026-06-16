/**
 * @file    WebServerManager.h
 * @brief   Web 服务器管理模块 — HTTP 重定向路由器
 *
 * @details 监听 80 端口，收到 GET / 请求时构造 GitHub Pages
 *          重定向 URL（附带设备局域网 IP 参数），
 *          返回 HTTP 302 将客户端引导至前端控制台页面。
 *          与 NetworkManager（WebSocket 81 端口）互不干扰。
 */

#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <ESP8266WebServer.h>

/**
 * @class WebServerManager
 * @brief 管理 ESP8266 HTTP 服务器（端口 80），将请求重定向至 GitHub Pages
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
     * @brief 注册重定向路由并启动 HTTP 服务
     *
     * @details 注册 GET / 路由：构造 GitHub Pages URL 并返回 302 重定向。
     *          URL 格式：https://kloms-fame.github.io/BitStream_12864/?ip=<局域网IP>
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
