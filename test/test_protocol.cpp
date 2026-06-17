/**
 * @file    test_protocol.cpp
 * @brief   [P3-3] 协议层单元测试 — 帧大小、亮度指令解析
 */

#include <Arduino.h>
#include <unity.h>
#include "NetworkManager.h"
#include "DisplayManager.h"
#include "DebugMacros.h"

/* ========================================================================== */
/*  帧格式常量验证                                                             */
/* ========================================================================== */

void test_frame_size_is_1028() {
    TEST_ASSERT_EQUAL(1028, NetworkManager::FRAME_SIZE);
}

void test_display_frame_bytes_is_1024() {
    TEST_ASSERT_EQUAL(1024, DisplayManager::FRAME_BYTES);
}

void test_frame_dimensions() {
    TEST_ASSERT_EQUAL(128, DisplayManager::FRAME_WIDTH);
    TEST_ASSERT_EQUAL(64, DisplayManager::FRAME_HEIGHT);
    // 128 * 64 / 8 = 1024
    TEST_ASSERT_EQUAL(1024, (DisplayManager::FRAME_WIDTH * DisplayManager::FRAME_HEIGHT) / 8);
}

/* ========================================================================== */
/*  亮度范围验证                                                               */
/* ========================================================================== */

void test_brightness_constants() {
    TEST_ASSERT_EQUAL(0, NetworkManager::BRIGHTNESS_MIN);
    TEST_ASSERT_EQUAL(255, NetworkManager::BRIGHTNESS_MAX);
    TEST_ASSERT_EQUAL(0, DisplayManager::BRIGHTNESS_MIN);
    TEST_ASSERT_EQUAL(255, DisplayManager::BRIGHTNESS_MAX);
    TEST_ASSERT_EQUAL(255, DisplayManager::BRIGHTNESS_DEFAULT);
}

/* ========================================================================== */
/*  WebSocket 端口验证                                                         */
/* ========================================================================== */

void test_ws_port() {
    TEST_ASSERT_EQUAL(81, NetworkManager::WS_PORT);
}

/* ========================================================================== */
/*  重试常量验证                                                               */
/* ========================================================================== */

void test_max_retries() {
    TEST_ASSERT_EQUAL(3, NetworkManager::MAX_RETRIES);
}

/* ========================================================================== */
/*  测试入口                                                                   */
/* ========================================================================== */

void setup() {
    delay(2000);
    UNITY_BEGIN();

    RUN_TEST(test_frame_size_is_1028);
    RUN_TEST(test_display_frame_bytes_is_1024);
    RUN_TEST(test_frame_dimensions);
    RUN_TEST(test_brightness_constants);
    RUN_TEST(test_ws_port);
    RUN_TEST(test_max_retries);

    UNITY_END();
}

void loop() {
    // PlatformIO test runner: no-op
}
