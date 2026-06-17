/**
 * @file    DisplayManager.cpp
 * @brief   DisplayManager 类的方法实现
 *
 * @details 包含 OLED 初始化、I2C 800kHz 超频、XBM 帧渲染、
 *          SSD1306 对比度亮度控制及状态文本居中显示。
 */

#include "DisplayManager.h"

/* ========================================================================== */
/*  构造函数                                                                  */
/* ========================================================================== */

DisplayManager::DisplayManager()
    : m_u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE)
    , m_brightness(BRIGHTNESS_DEFAULT)
{
}

/* ========================================================================== */
/*  begin() — I2C 超频 + 硬件初始化                                           */
/* ========================================================================== */

void DisplayManager::begin()
{
    // 步骤 1：将 I2C 时钟从默认 100kHz 提升至 800kHz
    // 效果：1024 字节帧传输从 ~90ms 压缩至 ~15ms，轻松匹配 20fps 推流
    m_u8g2.setBusClock(800000UL);

    // 步骤 2：初始化 U8g2 驱动（配置 SSD1306 寄存器、分配帧缓冲区）
    m_u8g2.begin();

    // 步骤 3：清空缓冲区并推送初始空白帧（消除上电残留噪点）
    m_u8g2.clearBuffer();
    m_u8g2.sendBuffer();

    // 步骤 4：设置为默认最大亮度
    setBrightness(BRIGHTNESS_DEFAULT);

    Serial.printf("[DISP] OLED 初始化完成 | I2C: 800kHz | 亮度: %u/255\n",
                  BRIGHTNESS_DEFAULT);
}

/* ========================================================================== */
/*  renderFrame() — 核心渲染出口（零延时，性能关键路径）                       */
/* ========================================================================== */

void DisplayManager::renderFrame(uint8_t *payload)
{
    // drawXBM 直接将 1024 字节位图写入 U8g2 内部缓冲区
    // 参数：起始坐标 (0,0)，尺寸 128×64，数据指针
    m_u8g2.drawXBM(0, 0, FRAME_WIDTH, FRAME_HEIGHT, payload);

    // sendBuffer 通过 800kHz I2C 一次性推送整帧至 SSD1306
    // 无延时、无阻塞等待 —— 追求极致绘制吞吐
    m_u8g2.sendBuffer();
}

/* ========================================================================== */
/*  setBrightness() — SSD1306 对比度寄存器控制                                 */
/* ========================================================================== */

void DisplayManager::setBrightness(uint8_t contrast)
{
    m_brightness = contrast;

    // SSD1306 对比度设置命令序列：
    //   0x81 → 对比度设置模式
    //   0xXX → 对比度值（0x00 = 最暗, 0xFF = 最亮）
    //
    // U8g2 的 setContrast() 内部封装了此命令序列，
    // 无需手动操作 I2C 寄存器
    m_u8g2.setContrast(contrast);
}

/* ========================================================================== */
/*  showStatus() — 居中状态文本                                                */
/* ========================================================================== */

void DisplayManager::showStatus(const char *text)
{
    m_u8g2.clearBuffer();

    // 6×10 像素终端风格字体，清晰可读
    m_u8g2.setFont(u8g2_font_6x10_tf);

    // 水平居中
    const uint8_t strWidth = m_u8g2.getStrWidth(text);
    const uint8_t x = (FRAME_WIDTH - strWidth) / 2;

    // 垂直居中（字体基线在字符下方，故偏移 10px 的一半 = 5px）
    const uint8_t y = FRAME_HEIGHT / 2 + 5;

    m_u8g2.drawStr(x, y, text);
    m_u8g2.sendBuffer();
}

/* ========================================================================== */
/*  getBrightness()                                                            */
/* ========================================================================== */

uint8_t DisplayManager::getBrightness() const
{
    return m_brightness;
}
