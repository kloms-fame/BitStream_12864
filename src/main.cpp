/**
 * @file    main.cpp
 * @brief   主程序入口 — 组装 DisplayManager / NetworkManager / WebServerManager
 *
 * @details 本文件将三个独立模块以积木方式组装为完整的二进制流媒体推送终端：
 *          1. WebServerManager  — 80 端口 HTTP 重定向至 GitHub Pages
 *          2. NetworkManager    — 81 端口接收 WebSocket 二进制帧
 *          3. DisplayManager    — 128×64 OLED 帧渲染（I2C 800kHz 超频）
 *
 *          loop() 中同时驱动 HTTP 与 WebSocket 两个事件循环，
 *          并在每轮迭代末尾调用 yield() 将控制权交还 ESP8266 WiFi 射频内核，
 *          确保高频推流下 TCP ACK 及时回复，防止电脑端 bufferedAmount 虚高。
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "DisplayManager.h"
#include "NetworkManager.h"
#include "WebServerManager.h"

/* ======================================================================== */
/*  全局模块实例                                                            */
/* ======================================================================== */

DisplayManager   display;
NetworkManager   network;
WebServerManager webServer;

/* ======================================================================== */
/*  setup — 系统初始化                                                      */
/* ======================================================================== */

void setup()
{
    Serial.begin(115200);

    // 1. 初始化 OLED（I2C 超频至 800kHz）并显示连接提示
    display.begin();
    display.showStatus("Connecting...");

    // 2. 连接 WiFi
    network.connectWiFi("Rennick_Plus", "password");

    // 3. 屏幕显示 IP 地址
    display.showStatus(WiFi.localIP().toString().c_str());

    // 4. 启动 HTTP 重定向服务（80 端口）
    webServer.begin();

    // 5. 启动 WebSocket 流媒体服务（81 端口）
    network.startStreamServer([](uint8_t *payload, size_t length)
    {
        display.renderFrame(payload);
    });
}

/* ======================================================================== */
/*  loop — 双路事件驱动 + 内核让渡                                          */
/* ======================================================================== */

void loop()
{
    webServer.loop();  // 处理 HTTP 请求
    network.loop();    // 处理 WebSocket 事件

    yield();           // 将控制权交还 WiFi 射频内核，确保 TCP ACK 及时回复
}