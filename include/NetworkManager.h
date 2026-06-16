/**
 * @file    NetworkManager.h
 * @brief   网络通信模块 — WiFi 连接 + WebSocket 二进制流接收
 *
 * @details 本模块将 WiFi 连接与 WebSocket 服务端收包逻辑封装为独立组件，
 *          通过 std::function 回调将解析后的帧数据解耦至上层业务模块。
 *
 * @warning 本模块假设 payload 被调用方立即消费或拷贝，不在内部做深拷贝，
 *          以避免在回调链路中引入不必要的堆分配。
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <functional>

/**
 * @class NetworkManager
 * @brief 管理 WiFi 连接与 WebSocket 服务端（端口 81）的二进制帧接收
 */
class NetworkManager
{
public:
    /**
     * @brief 构造 NetworkManager 实例
     *
     * @details 在初始化列表中创建监听 81 端口的 WebSocket 服务端对象。
     */
    NetworkManager();

    /**
     * @brief 以阻塞方式连接 WiFi 网络
     *
     * @details 调用 WiFi.begin() 后轮询等待连接完成，
     *          成功后通过串口打印 ESP8266 获取到的 IP 地址。
     *
     * @param ssid     目标 WiFi SSID
     * @param password 目标 WiFi 密码
     */
    void connectWiFi(const char *ssid, const char *password);

    /**
     * @brief 启动 WebSocket 服务器并注册帧接收回调
     *
     * @details 绑定 WebSocket 事件处理器。当收到类型为 WStype_BIN
     *          且长度恰好为 1024 字节的二进制帧时，调用 onFrameReceived
     *          将 payload 指针与长度传递给上层。其他事件静默忽略。
     *
     * @param onFrameReceived 帧接收回调函数，签名为 void(uint8_t*, size_t)
     */
    void startStreamServer(std::function<void(uint8_t *, size_t)> onFrameReceived);

    /**
     * @brief 驱动 WebSocket 事件循环
     *
     * @details 内部仅调用 webSocket.loop()，需在 main loop() 中高频调用。
     */
    void loop();

private:
    WebSocketsServer webSocket; ///< 监听 81 端口的 WebSocket 服务端实例
};

#endif // NETWORK_MANAGER_H
