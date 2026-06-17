/**
 * @file    DebugMacros.h
 * @brief   [P3-6] 编译期日志开关 — 节省 Flash 并减少串口中断
 *
 * @details 在生产构建中关闭高频日志（逐帧、WebSocket 事件），
 *          仅在关键事件（连接/断开/错误）保留串口输出。
 *
 *          通过 platformio.ini 的 build_flags 控制：
 *          -DENABLE_DEBUG_LOG    开启调试日志
 *          （不定义）             仅输出关键事件
 */

#ifndef DEBUG_MACROS_H
#define DEBUG_MACROS_H

#include <Arduino.h>

/* ── 编译期日志开关 ──────────────────────────────────────────────── */

#ifdef ENABLE_DEBUG_LOG
  /** 调试日志：仅当 ENABLE_DEBUG_LOG 定义时有效 */
  #define DEBUG_PRINTF(fmt, ...)  Serial.printf((fmt), ##__VA_ARGS__)
#else
  #define DEBUG_PRINTF(fmt, ...)  ((void)0)
#endif

/* ── 分级日志宏 ──────────────────────────────────────────────────── */

/** 逐帧路径日志（默认静默） */
#define FRAME_LOG(fmt, ...)   DEBUG_PRINTF(fmt, ##__VA_ARGS__)

/** WebSocket 事件日志（默认静默） */
#define WS_LOG(fmt, ...)      DEBUG_PRINTF(fmt, ##__VA_ARGS__)

/** 关键事件：连接/断开/错误/模式切换（始终输出） */
#define EVENT_LOG(fmt, ...)   Serial.printf((fmt), ##__VA_ARGS__)

#endif // DEBUG_MACROS_H
