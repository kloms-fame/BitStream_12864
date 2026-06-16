# Repository Guidelines

## 项目结构与模块组织

```
BitStream_12864/
├── src/          # 主程序源码 (main.cpp)
├── include/      # 公共头文件 (.h)
├── lib/          # 项目私有库
├── test/         # PlatformIO 单元测试
├── platformio.ini # 项目配置与依赖
└── 参考项目/      # 参考实现项目
```

- `src/main.cpp` — 程序入口（`setup()` / `loop()`）
- `include/` — 跨源文件共享的声明与宏定义
- `lib/` — 放置项目专属的 Arduino/PlatformIO 库
- `test/` — 使用 PlatformIO Unit Testing 框架编写的测试用例

## 构建、测试与开发命令

| 命令 | 说明 |
|---|---|
| `pio run` | 编译项目（环境：`nodemcuv2`） |
| `pio run -t upload` | 编译并上传至 ESP8266 开发板 |
| `pio test` | 运行 `test/` 中的单元测试 |
| `pio device monitor` | 打开串口监视器（波特率默认 115200） |

以上命令需在 `BitStream_12864/` 目录下执行，并确保已安装 [PlatformIO Core](https://platformio.org/install/cli)。

## 编码风格与命名规范

- **语言标准**：C++（Arduino 框架），使用 `#include <Arduino.h>`
- **缩进**：建议使用 2 空格缩进，与 PlatformIO 默认模板保持一致
- **函数命名**：驼峰式（camelCase），如 `myFunction()`
- **文件命名**：源文件 `.cpp`，头文件 `.h`
- 公共声明放在 `include/` 目录下对应的头文件中，通过 `#include "header.h"` 引用
- 使用 VSCode + PlatformIO 插件可获得 IntelliSense 与一键构建体验

## 测试指南

- 测试框架：**PlatformIO Unit Testing**
- 测试目录：`test/`，测试文件以 `test_` 前缀命名（如 `test_display.cpp`）
- 运行 `pio test` 即可在目标硬件或本地执行所有测试
- 若需特定环境执行，添加 `-e nodemcuv2` 参数

## 提交与 Pull Request 指南

- 提交前确保 `pio run` 编译通过
- 遵循常规提交格式：`类型: 简要描述`（如 `feat: 添加 WiFi 流传输模块`）
- `.pio/` 目录已被 `.gitignore` 排除，无需手动清理
- 参考项目（`参考项目/`）中保留的原始 README 与代码可作为设计参考，请勿直接提交参考项目中的代码

## 硬件与依赖

- **目标平台**：ESP8266 (NodeMCU v2)，SSD1306 128×64 OLED
- **框架**：Arduino，依赖库通过 PlatformIO Library Manager 管理
- 在 `platformio.ini` 的 `lib_deps` 中声明外部库依赖
