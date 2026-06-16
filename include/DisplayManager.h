/**
 * @file    DisplayManager.h
 * @brief   OLED 显示管理模块 — 封装 U8g2 驱动的所有显示操作
 *
 * @details 本模块基于“高内聚、低耦合”原则设计，将 SSD1306 OLED 的初始化、
 *          I2C 超频配置、状态提示及二进制帧渲染全部收敛于单一类中。
 *          外部使用者无需直接接触 U8g2 底层 API，仅通过简洁的公有接口即可完成全部显示任务。
 *
 *          I2C 总线在 begin() 中自动超频至 800kHz（默认 100kHz），
 *          将单帧传输时间从 ~90ms 压缩至 ~15ms。
 *
 * @warning 本模块不包含任何与网络相关的代码，仅负责像素级显示控制。
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>

/**
 * @class DisplayManager
 * @brief 统一管理 SSD1306 128×64 OLED 显示屏的初始化、I2C 超频与帧渲染
 */
class DisplayManager
{
public:
    /**
     * @brief 构造 DisplayManager 实例
     *
     * @details 在构造阶段即创建 SSD1306 硬件 I2C 的 U8g2 对象。
     *          使用的构造函数参数：
     *          - U8G2_R0：不旋转
     *          - U8X8_PIN_NONE：使用默认硬件 I2C 引脚 (GPIO4=SDA, GPIO5=SCL)
     */
    DisplayManager();

    /**
     * @brief 初始化 OLED 硬件并超频 I2C 总线
     *
     * @details 先将 I2C 时钟频率提升至 800kHz，再调用 U8g2 begin() 与
     *          clearBuffer()，确保显示屏进入高速可操作状态且缓冲区已清零。
     */
    void begin();

    /**
     * @brief 在屏幕中央显示状态文本
     *
     * @details 清空缓冲区后在屏幕垂直居中位置绘制指定文本，
     *          随后发送至显示屏。适用于 WiFi 连接提示、错误信息等
     *          简短状态展示。
     *
     * @param text 要显示的 C 风格字符串（建议不超过 16 个 ASCII 字符）
     */
    void showStatus(const char *text);

    /**
     * @brief 渲染一帧二进制位图流（零延时）
     *
     * @details 接收 1024 字节的纯二进制位图数据（128 列×64 行÷8 位），
     *          直接调用 U8g2 的 drawXBM() 将位图写入缓冲区，
     *          随即通过超频 I2C 总线调用 sendBuffer() 将整帧推送到 OLED 屏幕。
     *          该方法是整个流媒体推送管线中的核心渲染出口。
     *
     * @param payload 指向 1024 字节位图数组的指针
     *
     * @note 调用者需确保 payload 指向合法的 1024 字节内存区域，
     *       本方法不做长度校验以保持零开销。
     *       本方法内部无任何延时，追求极致绘制速度。
     */
    void renderFrame(uint8_t *payload);

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2; ///< U8g2 显示器驱动实例
};

#endif // DISPLAY_MANAGER_H