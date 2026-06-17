/**
 * @file    DisplayManager.h
 * @brief   V2 显示内核 — SSD1306 OLED 超频渲染与亮度控制
 *
 * @details 本模块是 V2 架构的"显示层"，严格遵循高内聚低耦合原则：
 *          - 零网络依赖：不包含任何 WiFi/WebSocket/HTTP 代码
 *          - 零业务逻辑：仅提供像素级渲染原语
 *          - I2C 800kHz 超频：将 1024 字节帧传输时间从 ~90ms 压缩至 ~15ms
 *          - 亮度控制：通过 SSD1306 对比度寄存器 (0x81) 实现 256 级软件调光
 *
 *          外部使用者仅通过以下 4 个公有接口即可完成全部显示任务：
 *          begin() → showStatus() → renderFrame() → setBrightness()
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>

/**
 * @class DisplayManager
 * @brief 统一管理 SSD1306 128×64 OLED 的硬件初始化、像素渲染与亮度控制
 *
 *        硬件规格：
 *        - 控制器：SSD1306
 *        - 分辨率：128×64 像素
 *        - 接口：硬件 I2C（GPIO4=SDA, GPIO5=SCL）
 *        - 帧格式：XBM 位图，1024 字节 (128×64÷8)
 *        - I2C 速率：800kHz（默认 100kHz，8 倍超频）
 */
class DisplayManager
{
public:
    /** @brief 亮度范围常量 */
    static constexpr uint8_t BRIGHTNESS_MIN  = 0;    ///< 最低亮度（几乎不可见）
    static constexpr uint8_t BRIGHTNESS_MAX  = 255;  ///< 最高亮度（默认值）
    static constexpr uint8_t BRIGHTNESS_DEFAULT = 255;
    static constexpr uint8_t FRAME_WIDTH     = 128;
    static constexpr uint8_t FRAME_HEIGHT    = 64;
    static constexpr uint16_t FRAME_BYTES    = 1024; ///< 128×64÷8

    /**
     * @brief 构造 DisplayManager 实例
     *
     * @details 在初始化列表中创建 SSD1306 硬件 I2C 的 U8g2 对象。
     *          参数含义：
     *          - U8G2_R0：屏幕不旋转
     *          - U8X8_PIN_NONE：使用 ESP8266 默认 I2C 引脚 (SDA=GPIO4, SCL=GPIO5)
     */
    DisplayManager();

    /**
     * @brief 初始化 OLED 硬件并超频 I2C 总线至 800kHz
     *
     * @details 执行顺序：
     *          1. 将 I2C 时钟从默认 100kHz 提升至 800kHz
     *          2. 调用 U8g2 硬件初始化
     *          3. 清空帧缓冲区
     *          4. 推送空白帧至屏幕（消除上电残留噪点）
     *
     * @note 若 800kHz 在长排线场景下不稳定，可降级为 400000UL
     */
    void begin();

    /**
     * @brief 渲染一帧二进制位图（零延时，极速绘制）
     *
     * @param payload 指向 1024 字节 XBM 位图数组的指针
     *
     * @details 将 128×64 单色位图数据通过 drawXBM() 写入 U8g2 缓冲区，
     *          随即通过 800kHz I2C 调用 sendBuffer() 一次性刷新整个屏幕。
     *          本方法是推流管线中的核心渲染出口。
     *
     * @warning 调用者需确保 payload 指向合法且完整的 1024 字节内存。
     *          本方法不做长度校验以保持零开销 —— 这是性能关键路径。
     */
    void renderFrame(uint8_t *payload);

    /**
     * @brief 动态调节 OLED 亮度（通过 SSD1306 对比度寄存器）
     *
     * @param contrast 亮度值，范围 [BRIGHTNESS_MIN, BRIGHTNESS_MAX]
     *
     * @details 向 SSD1306 发送对比度设置命令 (0x81)，
     *          值越大显示越亮。0 为几乎不可见，255 为出厂默认最大亮度。
     *          这是一种纯软件调光方式，不依赖外部 PWM 或限流电路。
     *
     * @note 此方法可在推流过程中实时调用，不影响帧渲染管线。
     */
    void setBrightness(uint8_t contrast);

    /**
     * @brief 在屏幕中央显示状态文本
     *
     * @param text C 风格字符串（建议不超过 18 个 ASCII 字符）
     *
     * @details 清空缓冲区后以 6×10 像素字体绘制水平居中、
     *          垂直居中的文本。适用于连接状态、错误信息等简短提示。
     *          此方法会覆盖当前帧内容。
     */
    void showStatus(const char *text);

    /**
     * @brief 获取当前亮度值
     *
     * @return 当前对比度设置值 [0, 255]
     */
    uint8_t getBrightness() const;

    // ── [P2-4] I2C 速率动态配置 ──────────────────────────────
    /**
     * @brief 运行时调整 I2C 总线时钟频率
     * @param hz 频率 (Hz), 例如 400000, 800000
     */
    void setBusClock(uint32_t hz);

    // ── [P2-4] 脏页检测帧渲染 ─────────────────────────────────
    /**
     * @brief 脏页检测渲染：仅当帧数据与上一帧不同时才执行 I2C 传输
     * @param payload 1024 字节 XBM 位图
     * @note 内部维护前一帧副本，首次调用执行全帧渲染
     */
    void renderFrameDirty(uint8_t *payload);

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C m_u8g2;  ///< U8g2 显示器驱动实例
    uint8_t m_brightness;                          ///< 当前亮度值缓存

    // [P2-4] 脏页检测
    uint8_t m_prevFrame[1024];  ///< 上一帧数据（用于 XOR 对比）
    bool    m_hasPrevFrame;     ///< 是否已有前一帧（首次全帧渲染）
};

#endif // DISPLAY_MANAGER_H
