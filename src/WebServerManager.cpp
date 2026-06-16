/**
 * @file    WebServerManager.cpp
 * @brief   WebServerManager 类的方法实现
 *
 * @details GET / 路由注册及 HTTP 302 重定向逻辑。
 */

#include "WebServerManager.h"
#include <ESP8266WiFi.h>

/* ======================================================================== */
/*  构造函数                                                                */
/* ======================================================================== */

/**
 * @brief 构造函数：创建 80 端口 HTTP 服务器实例
 */
WebServerManager::WebServerManager()
    : server(80)
{
}

/* ======================================================================== */
/*  公有方法                                                                */
/* ======================================================================== */

/**
 * @brief 注册根路径重定向路由并启动 HTTP 服务
 *
 * @details 注册 GET / 处理器：构造 GitHub Pages URL，
 *          将设备局域网 IP 作为查询参数附加，
 *          通过 HTTP 302 重定向将客户端引导至前端控制台。
 */
void WebServerManager::begin()
{
    server.on("/", HTTP_GET, [this]()
    {
        String url = "https://kloms-fame.github.io/BitStream_12864/?ip="
                   + WiFi.localIP().toString();
        server.sendHeader("Location", url, true);
        server.send(302, "text/plain", "");
    });

    server.begin();
    Serial.println("HTTP redirect server started on port 80");
}

/**
 * @brief 驱动 HTTP 服务器事件循环
 */
void WebServerManager::loop()
{
    server.handleClient();
}
