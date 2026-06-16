/**
 * @file    WebServerManager.cpp
 * @brief   WebServerManager 类的方法实现
 *
 * @details LittleFS 挂载、GET / 路由注册及 HTTP 事件循环。
 */

#include "WebServerManager.h"

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
 * @brief 挂载 LittleFS 并注册根路径路由
 *
 * @details 调用 LittleFS.begin() 挂载闪存文件系统。
 *          注册 GET / 处理器：从 LittleFS 打开 /index.html，
 *          显式声明 text/html; charset=utf-8 防止中文乱码，
 *          以流式方式返回文件内容避免 String 堆分配。
 */
void WebServerManager::begin()
{
    LittleFS.begin();

    server.on("/", HTTP_GET, [this]()
    {
        File file = LittleFS.open("/index.html", "r");
        if (!file)
        {
            server.send(404, "text/plain", "404: index.html not found");
            return;
        }

        server.sendHeader("Content-Type", "text/html; charset=utf-8");
        server.streamFile(file, "");
        file.close();
    });

    server.begin();
    Serial.println("HTTP server started on port 80");
}

/**
 * @brief 驱动 HTTP 服务器事件循环
 */
void WebServerManager::loop()
{
    server.handleClient();
}
