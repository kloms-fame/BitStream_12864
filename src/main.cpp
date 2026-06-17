/**
 * @file    main.cpp
 * @brief   BitStream V2 — 组装层（Composition Root）
 *
 * @details 三模块零耦合组装：
 *          NetworkManager (WiFi STA/AP + WebSocket:81)
 *          WebServerManager (HTTP:80 + DNS:53 + Captive Portal)
 *          DisplayManager (OLED 800kHz XBM 渲染)
 *
 *          AP 模式实时反馈：通过 WiFiEventHandler 监听客户端接入/断开，
 *          立即更新 OLED 屏幕和串口日志。
 *
 * @warning loop() 底部必须保留 delay(1) + yield()，
 *          这是 ESP8266 单核 WiFi 栈不被推流饿死的唯一保障。
 */

#include <Arduino.h>
#include "DebugMacros.h"  // [P3-6]
#include <ESP8266WiFi.h>
#include "NetworkManager.h"
#include "WebServerManager.h"
#include "DisplayManager.h"

/* ======================================================================== */
/*  全局模块实例（文件作用域，构造即初始化，零堆分配）                          */
/* ======================================================================== */

NetworkManager  network;
WebServerManager webServer;
DisplayManager  display;

/* ======================================================================== */
/*  [P1-1] 单帧覆盖缓冲 — WebSocket 回调快速写入，loop() 逐帧出队渲染         */
/*  始终只保留最新一帧，天然丢弃过期帧，避免"倒带式补帧"                         */
/* ======================================================================== */

static uint8_t  s_pendingFrame[1024];
static uint32_t s_pendingSeq  = 0;
static bool     s_hasNewFrame = false;

static uint32_t s_lastFrameTime = 0;  // [P3-2] 看门狗

/* ======================================================================== */
/*  AP 客户端事件监听器（静态句柄，防止 Lambda 捕获被过早释放）                */
/* ======================================================================== */

static WiFiEventHandler apConnHandler;
static WiFiEventHandler apDisconnHandler;

/* ======================================================================== */
/*  setup                                                                    */
/* ======================================================================== */

void setup()
{
    /* ---- 0. 串口 ------------------------------------------------------ */
    Serial.begin(115200);
    delay(300);
    Serial.println(F("\n============================================"));
    Serial.println(F("  BitStream V2 - ESP8266 OLED Streamer      "));
    Serial.println(F("============================================"));
    Serial.printf ("[MAIN] 启动时间: %lu ms | 复位原因: %s\n",
                   millis(), ESP.getResetReason().c_str());

    /* ---- 1. 显示内核最先启动 ------------------------------------------ */
    Serial.println(F("[MAIN] Phase 1/4: 显示内核初始化"));
    display.begin();
    display.showStatus("Booting...");

    /* ---- 2. 网络内核启动（内部阻塞最多 45s STA 重试 / 直接进入 AP）--- */
    Serial.println(F("[MAIN] Phase 2/4: 网络内核启动"));
    network.begin(
        [](uint8_t *payload, size_t /*length*/) {
            // [P1-1] 仅写入缓冲，渲染在 loop() 中异步执行
            memcpy(s_pendingFrame, payload, 1024);
            s_hasNewFrame = true;
            s_lastFrameTime = millis();  // [P3-2] 看门狗喂狗
        },
        [](uint8_t brightness) {
            display.setBrightness(brightness);
        }
    );

    /* ---- 3. 注册 AP 客户端事件 — OLED + 串口实时反馈 ----------------- */
    apConnHandler = WiFi.onSoftAPModeStationConnected(
        [](const WiFiEventSoftAPModeStationConnected& evt) {
            Serial.printf("[MAIN] 设备接入 AP! "
                          "MAC: %02x:%02x:%02x:%02x:%02x:%02x, AID: %d\n",
                          MAC2STR(evt.mac), evt.aid);
            display.showStatus("Client Connected!\n192.168.4.1");
        });

    apDisconnHandler = WiFi.onSoftAPModeStationDisconnected(
        [](const WiFiEventSoftAPModeStationDisconnected& evt) {
            Serial.printf("[MAIN] 设备断开 AP! "
                          "MAC: %02x:%02x:%02x:%02x:%02x:%02x, AID: %d\n",
                          MAC2STR(evt.mac), evt.aid);
            if (WiFi.softAPgetStationNum() == 0)
            {
                display.showStatus("AP:OLED-BitStream");
            }
        });

    /* ---- 4. Web 服务启动（根据 network 的 AP/STA 状态动态配置路由）--- */
    Serial.println(F("[MAIN] Phase 3/4: Web服务启动"));
    webServer.begin();

    /* ---- 5. OLED 显示当前状态 ---------------------------------------- */
    Serial.println(F("[MAIN] Phase 4/4: 启动完成，进入主循环"));
    if (NetworkManager::isAPMode())
    {
        display.showStatus("AP:OLED-BitStream");
        Serial.println(F("[MAIN] AP 模式 — 热点: OLED-BitStream (无密码)"));
        Serial.println(F("[MAIN] 请连接热点，浏览器将自动弹出离线控制台"));
    }
    else
    {
        const String ip = WiFi.localIP().toString();
        display.showStatus(ip.c_str());
        Serial.printf("[MAIN] STA 已连接 — IP: %s\n", ip.c_str());
        Serial.printf("[MAIN] 访问 http://%s/ 自动跳转至控制台\n", ip.c_str());
        delay(1200);
    }

    Serial.printf("[MAIN] WebSocket 服务就绪 — 端口 %u\n", NetworkManager::WS_PORT);
}

/* ======================================================================== */
/*  loop                                                                     */
/* ======================================================================== */


/* ======================================================================== */
/*  renderPendingFrame() — 从单帧缓冲出队并渲染（零帧不画）                 */
/* ======================================================================== */
static void renderPendingFrame() {
    if (!s_hasNewFrame) return;
    display.renderFrame(s_pendingFrame);
    s_hasNewFrame = false;
}

void loop()
{
    network.loop();   // WebSocket 帧接收 + 指令解析 + ACK 锁步
    webServer.loop(); // HTTP 路由 + DNS 劫持
    renderPendingFrame();  // [P1-1] 异步渲染最新帧

    /* ---- 每30秒输出系统心跳 ------------------------------------------ */
    static uint32_t s_lastBeat = 0;
    const uint32_t now = millis();
    if (now - s_lastBeat >= 30000) {
        s_lastBeat = now;
        Serial.printf("[MAIN] 心跳 | 运行: %u s | 堆: %u B | 模式: %s | WS客户端: %u\n",
                      now / 1000, ESP.getFreeHeap(),
                      NetworkManager::isAPMode() ? "AP" : "STA",
                      network.getClientCount());
    }


    /* ── [P3-2] 看门狗：超过 10 秒无帧 → 软复位 ──────────────
     *  仅在推流活跃时（hasNewFrame 曾被设置过）启用，
     *  避免刚启动时误触发。
     */
    if (s_lastFrameTime > 0 && (millis() - s_lastFrameTime > 10000)) {
        EVENT_LOG("[MAIN] 看门狗：10 秒无帧，软复位...\n");
        delay(100);
        ESP.restart();
    }

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
