/**
 * @file    main.cpp
 * @brief   BitStream V2 — 阶段二：极速显示与流媒体接收层
 *
 * @details 本文件是 V2 架构的组装层（Composition Root），
 *          将三个完全独立的模块通过 std::function 回调进行解耦连接：
 *
 *          ┌─────────────────────────────────────────────────────┐
 *          │                    main.cpp                          │
 *          │                                                     │
 *          │  ConfigManager    ←  WiFi 配网 / HTTP 302 重定向     │
 *          │       │                                             │
 *          │  DisplayManager   ←  OLED 800kHz I2C 渲染 + 亮度    │
 *          │       ▲               ▲                             │
 *          │       │  renderFrame  │ setBrightness               │
 *          │       │               │                             │
 *          │  NetworkManager   ←  WebSocket:81 帧接收 + 指令     │
 *          │       └──── std::function 回调 ────┘                │
 *          └─────────────────────────────────────────────────────┘
 *
 *          模块间零直接依赖 — DisplayManager 不知道网络的存在，
 *          NetworkManager 不知道 OLED 的存在。所有连接均在 main.cpp
 *          中以 Lambda 表达式的形式显式声明。
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "ConfigManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"

/* ======================================================================== */
/*  全局模块实例                                                            */
/* ======================================================================== */

ConfigManager  config;    ///< WiFi 配网 + HTTP 重定向（阶段一）
DisplayManager display;   ///< OLED 渲染内核（阶段二）
NetworkManager network;   ///< WebSocket 通信内核（阶段二）

/* ======================================================================== */
/*  setup — 分层初始化                                                        */
/* ======================================================================== */

void setup()
{
    Serial.begin(115200);
    delay(500);

    // ── 第 1 层：显示内核 — 最先启动以便展示状态 ─────────────────────
    display.begin();
    display.showStatus("BitStream V2");
    delay(800);
    display.showStatus("Connecting...");

    // ── 第 2 层：配网模块 — 自动决策 AP 模式或 Station 连接 ──────────
    //    内部可能阻塞最多 15 秒等待 WiFi 连接
    config.begin();

    // ── 第 3 层：根据配网结果决定后续路径 ────────────────────────────
    if (config.isWiFiConnected())
    {
        // WiFi 已连接 → 显示 IP 并启动流媒体服务

        const String ipStr = WiFi.localIP().toString();
        display.showStatus(ipStr.c_str());
        Serial.printf("\n[MAIN] 设备就绪 — IP: %s\n", ipStr.c_str());

        delay(1500); // 让用户有时间看到 IP

        // ── 第 4 层：启动 WebSocket 流媒体服务 ──────────────────────
        //     通过 std::function 回调将 NetworkManager 与 DisplayManager 解耦连接
        //
        //     帧回调：  NetworkManager 收到 1024B 二进制帧
        //              → DisplayManager::renderFrame（OLED 渲染）
        //              → NetworkManager 内部自动发送 "ACK"（锁步确认）
        //
        //     亮度回调：NetworkManager 收到 "BRIGHTNESS:128"
        //              → DisplayManager::setBrightness（OLED 对比度调节）
        //              → NetworkManager 内部自动发送 "OK"（指令确认）

        network.begin(
            // ── 帧渲染回调 ──────────────────────────────────────────
            [](uint8_t *payload, size_t length)
            {
                display.renderFrame(payload);
            },
            // ── 亮度控制回调 ────────────────────────────────────────
            [](uint8_t brightness)
            {
                display.setBrightness(brightness);
            }
        );

        Serial.printf("[MAIN] 流媒体服务已启动 — WebSocket 端口 %u\n",
                      NetworkManager::WS_PORT);
        Serial.println(F("[MAIN] 等待前端推流连接..."));
    }
    else
    {
        // AP 配网模式 — 显示热点名称
        display.showStatus("AP Mode");
        delay(1000);
        display.showStatus("OLED-BitStream");
        Serial.println(F("[MAIN] AP 配网模式 — 等待用户连接热点"));
    }
}

/* ======================================================================== */
/*  loop — 双路事件驱动                                                      */
/* ======================================================================== */

void loop()
{
    // 第 1 路：ConfigManager 事件循环
    //   - Station 模式：驱动 HTTP 302 重定向服务（80 端口）
    //   - AP 模式：驱动 DNS 劫持（53 端口）+ Captive Portal HTTP（80 端口）
    //   - AP 模式超时检测（5 分钟无配网自动重启）
    config.loop();

    // 第 2 路：NetworkManager 事件循环
    //   - 驱动 WebSocket 服务（81 端口）
    //   - 接收二进制帧 → 触发帧回调 → 发送 ACK
    //   - 接收文本指令 → 触发亮度回调 → 发送 OK
    //   - 仅在 Station 模式下有实际客户端；AP 模式下 webSocket.loop() 无害
    network.loop();

    // 极短延时 + yield：确保 ESP8266 WiFi 射频栈有足够时间处理 TCP ACK
    // 这对 WebSocket 锁步机制的实时性至关重要
    delay(1);
    yield();
}
