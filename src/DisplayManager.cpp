/**
 * @file    DisplayManager.cpp
 * @brief   DisplayManager 类的方法实现
 *
 * @details 包含 OLED 初始化、I2C 超频配置、状态提示及二进制帧渲染。
 *          依赖 U8g2lib 库与硬件 I2C 总线。
 *          I2C 时钟超频至 800kHz，将单帧传输时间从 ~90ms 压缩至 ~15ms，
 *          以匹配 20fps（50ms/帧）的推流速率。
 */

#include "DisplayManager.h"

/* ======================================================================== */
/*  构造函数                                                                */
/* ======================================================================== */

/**
 * @brief 构造函数：初始化 U8g2 SSD1306 对象
 *
 * @details 使用 SSD1306 128×64 全缓冲区模式的硬件 I2C 配置。
 *          姿态为 U8G2_R0（无旋转），引脚使用 U8X8_PIN_NONE 即 ESP8266
 *          默认 I2C 引脚：GPIO4 (SDA) 与 GPIO5 (SCL)。
 */
DisplayManager::DisplayManager()
    : u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE)
{
}

/* ======================================================================== */
/*  公有方法                                                                */
/* ======================================================================== */

/**
 * @brief 执行 OLED 硬件初始化与 I2C 总线超频
 *
 * @details 在调用 begin() 之前将 I2C 时钟频率从默认 100kHz 提升至 800kHz，
 *          将 1024 字节帧数据的传输时间从 ~90ms 压缩至 ~15ms，
 *          确保 20fps 推流不产生缓冲区堆积。
 *          若 800kHz 不稳定可降级为 400000。
 */
void DisplayManager::begin()
{
    u8g2.setBusClock(800000);
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

/**
 * @brief 在 OLED 屏幕中央显示一行状态文本
 *
 * @param text C 风格字符串，建议不超过 16 个 ASCII 字符
 *
 * @details 先清空缓冲区，设定字体为标准 8px 高度字体，
 *          将文本绘制在水平居中、垂直居中的位置，最后推送至屏幕。
 */
void DisplayManager::showStatus(const char *text)
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);

    // 计算居中坐标
    uint8_t strWidth  = u8g2.getStrWidth(text);
    uint8_t x         = (128 - strWidth) / 2;
    uint8_t y         = 64 / 2;

    u8g2.drawStr(x, y, text);
    u8g2.sendBuffer();
}

/**
 * @brief 渲染一帧 1024 字节的 XBM 位图流（零延时，极速绘制）
 *
 * @param payload 指向 1024 字节位图数据的内存指针
 *
 * @details 将 payload 指向的 128×64 单色位图数据通过 drawXBM()
 *          写入 U8g2 调试缓冲区，随后调用 sendBuffer() 一次性
 *          通过超频 I2C 总线刷新整个 OLED 屏幕。
 *          本方法内部无任何延时逻辑，追求极致绘制速度。
 */
void DisplayManager::renderFrame(uint8_t *payload)
{
    u8g2.drawXBM(0, 0, 128, 64, payload);
    u8g2.sendBuffer();
}