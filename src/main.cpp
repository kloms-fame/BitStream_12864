/**
 * @file    main.cpp
 * @brief   BitStream V2 — 组装层（Composition Root）
 *
 * @details 本文件是 V2 架构唯一的"胶水层"。
 *          三个模块（Config / Display / Network）彼此完全陌生，
 *          仅在 main.cpp 中通过 std::function 回调完成接线。
 *
 *          启动序列：
 *          Serial → Display("Booting...") → Config.begin() → Display(IP) → Network.begin()
 *
 *          ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
 *          │ConfigManager │    │DisplayManager│    │NetworkManager│
 *          │ WiFi / HTTP  │    │ OLED 800kHz  │    │ WebSocket:81 │
 *          └──────┬───────┘    └──────▲───────┘    └──────┬───────┘
 *                 │                  │  renderFrame       │
 *                 │                  │  setBrightness     │
 *                 │             ┌────┴────┐              │
 *                 │             │ main.cpp │◄─────────────┘
 *                 │             │  Lambda  │
 *                 └─────────────┤ 接线盒   │
 *                               └──────────┘
 *
 * @warning loop() 底部必须保留 delay(1) + yield()，
 *          这是 ESP8266 单核 WiFi 栈不被推流饿死的唯一保障。
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"

/* ======================================================================== */
/*  全局模块实例（文件作用域，构造即初始化，零堆分配）                          */
/* ======================================================================== */

ConfigManager  config;
DisplayManager display;
NetworkManager network;

/* ======================================================================== */
/*  setup                                                                    */
/* ======================================================================== */

void setup()
{
    /* ---- 0. 串口 ------------------------------------------------------ */
    Serial.begin(115200);
    delay(300);

    /* ---- 1. 显示内核最先启动（后续所有状态通过 OLED 可视化）----------- */
    display.begin();
    display.showStatus("Booting...");

    /* ---- 2. 网络配网（内部阻塞最长 15s 等待 WiFi）-------------------- */
    config.begin();

    /* ---- 3. 分支：已连接 → 推流模式 / 未连接 → AP 配网模式 ----------- */
    if (config.isWiFiConnected())
    {
        /* -- 3a. 显示本机 IP ------------------------------------------ */
        const String ip = WiFi.localIP().toString();
        display.showStatus(ip.c_str());
        Serial.printf("[MAIN] WiFi 已连接, IP: %s\n", ip.c_str());
        delay(1200);

        /* -- 3b. 启动 WebSocket 流媒体服务 ----------------------------
         *  通过两个 Lambda 完成模块间解耦接线：
         *  - 帧回调：  NetworkManager → DisplayManager::renderFrame
         *  - 亮度回调：NetworkManager → DisplayManager::setBrightness
         *  NetworkManager 不知道 OLED 存在，DisplayManager 不知道网络存在。
         */
        network.begin(
            [](uint8_t *payload, size_t /*length*/)
            { display.renderFrame(payload); },
            [](uint8_t brightness)
            { display.setBrightness(brightness); }
        );

        Serial.printf("[MAIN] WebSocket 服务就绪, 端口 %u\n",
                      NetworkManager::WS_PORT);
    }
    else
    {
        /* -- 3c. AP 配网模式 ------------------------------------------ */
        display.showStatus("AP:OLED-BitStream");
        Serial.println("[MAIN] AP 配网模式, 热点: OLED-BitStream");
    }
}

/* ======================================================================== */
/*  loop                                                                     */
/* ======================================================================== */

void loop()
{
    config.loop();   // HTTP 重定向 + DNS 劫持 + 超时检测
    network.loop();  // WebSocket 帧接收 + 指令解析 + ACK 锁步

    /* ── 防卡死：确保 ESP8266 WiFi 射频栈能及时处理 TCP ACK ──────────
     *   ESP8266 是单核芯片，WiFi 协议栈与用户代码共享 CPU。
     *   若 loop() 不主动让出时间片，TCP 收包队列将堆积，
     *   导致发送端 bufferedAmount 虚高、延迟雪崩。
     *   delay(1) 将 CPU 让渡 1ms → WiFi 栈处理收发包 → 返回用户代码。
     *   yield() 是 backup，确保更底层的后台任务也能执行。
     */
    delay(1);
    yield();
}
