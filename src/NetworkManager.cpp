/**
 * @file    NetworkManager.cpp
 * @brief   NetworkManager 类的方法实现
 *
 * @details WiFi 阻塞连接、WebSocket 帧接收回调注册及事件循环的完整实现。
 */

#include "NetworkManager.h"

/* ======================================================================== */
/*  构造函数                                                                */
/* ======================================================================== */

/**
 * @brief 构造函数：在初始化列表中创建 81 端口 WebSocket 服务端
 */
NetworkManager::NetworkManager()
    : webSocket(81)
{
}

/* ======================================================================== */
/*  公有方法                                                                */
/* ======================================================================== */

/**
 * @brief 以阻塞方式连接指定 WiFi 网络
 *
 * @param ssid     目标 SSID
 * @param password 目标密码
 *
 * @details 调用 WiFi.begin() 后在循环中等待状态变为 WL_CONNECTED。
 *          连接成功后通过 Serial 输出本机 IP 地址。
 */
void NetworkManager::connectWiFi(const char *ssid, const char *password)
{
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    Serial.print("WiFi connected - IP: ");
    Serial.println(WiFi.localIP());
}

/**
 * @brief 启动 WebSocket 服务器并注册帧接收回调
 *
 * @param onFrameReceived 上层回调，签名为 void(uint8_t*, size_t)
 *
 * @details 调用 webSocket.begin() 后绑定事件处理器，通过 Serial
 *          输出客户端连接/断开及异常帧类型日志以便诊断。
 */
void NetworkManager::startStreamServer(std::function<void(uint8_t *, size_t)> onFrameReceived)
{
    webSocket.begin();
    Serial.println("WebSocket server started on port 81");

    webSocket.onEvent([this, onFrameReceived](uint8_t num, WStype_t type, uint8_t *payload, size_t length)
    {
        switch (type)
        {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Client disconnected\n", num);
            break;

        case WStype_CONNECTED:
            Serial.printf("[%u] Client connected from %s\n",
                          num,
                          webSocket.remoteIP(num).toString().c_str());
            break;

        case WStype_BIN:
            if (length == 1024)
            {
                onFrameReceived(payload, length);
            }
            else
            {
                Serial.printf("[%u] BIN frame ignored: expected 1024, got %u bytes\n",
                              num, length);
            }
            break;

        default:
            // 静默忽略其他事件类型（PING/PONG/TEXT 等）
            break;
        }
    });
}

/**
 * @brief 驱动 WebSocket 内部事件循环
 *
 * @details 仅转发至 webSocket.loop()，需在 Arduino loop() 中周期性调用。
 */
void NetworkManager::loop()
{
    webSocket.loop();
}
