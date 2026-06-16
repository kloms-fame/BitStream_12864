/**
 * @file    main.cpp
 * @brief   主程序入口 — 组装 DisplayManager 与 NetworkManager
 *
 * @details 本文件将两个独立模块以“积木拼接”方式组装为完整的
 *          二进制流媒体推送终端：
 *          1. 初始化 OLED 屏幕，显示连接提示
 *          2. 连接 WiFi，成功后将 IP 地址输出到屏幕
 *          3. 启动 WebSocket 服务端，接收帧数据并渲染至屏幕
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "DisplayManager.h"
#include "NetworkManager.h"

/* ======================================================================== */
/*  全局模块实例                                                            */
/* ======================================================================== */

DisplayManager display;
NetworkManager network;

/* ======================================================================== */
/*  setup — 系统初始化                                                      */
/* ======================================================================== */

void setup()
{
    Serial.begin(115200);

    // 1. 初始化 OLED 并显示连接提示
    display.begin();
    display.showStatus("Connecting...");

    // 2. 连接 WiFi
    network.connectWiFi("Rennick_Plus", "password");

    // 3. 屏幕显示 IP 地址
    display.showStatus(WiFi.localIP().toString().c_str());

    // 4. 启动 WebSocket 流媒体服务
    network.startStreamServer([](uint8_t *payload, size_t length)
    {
        display.renderFrame(payload);
    });
}

/* ======================================================================== */
/*  loop — 事件驱动                                                         */
/* ======================================================================== */

void loop()
{
    network.loop();
}
