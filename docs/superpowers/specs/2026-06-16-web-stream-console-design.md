# Web 推流控制台 — 设计文档

**日期:** 2026-06-16  
**状态:** 已确认

## 概述

单文件纯前端 HTML 页面，通过浏览器直连 ESP8266 WebSocket（81 端口），将本地视频实时二值化为 128×64 XBM 二进制帧并推流。

## 架构

\\\
[本地视频文件] → [<video> 静音播放]
                        ↓ 每 50ms
              [隐藏 Canvas 128×64]
                        ↓ ImageData RGBA
              [灰度二值化 阈值128]
                        ↓
              [XBM 打包 1024字节]
                    ↙           ↘
         [预览Canvas 128×64]   [WebSocket → ESP8266 OLED]
\\\

## 组件

### 1. 连接面板
- IP 地址输入框（ws://IP:81 格式提示）
- 连接/断开按钮
- WebSocket 状态指示灯（绿=已连接，红=已断开）

### 2. 控制面板
- 视频文件选取（<input type="file" accept="video/*">）
- <video> 标签（muted, loop, 隐藏样式）
- "开始推流" / "停止推流" 按钮

### 3. 预览面板
- 可见 <canvas> 128×64 画布
- CSS 放大至 512×256 便于肉眼观察
- 实时渲染二值化后的黑白像素

## 数据协议

- **WebSocket binaryType:** "arraybuffer"
- **帧格式:** 1024 字节 Uint8Array，XBM 格式
- **像素序:** 横向 LSB 优先（字节 bit 0 = 最左像素）
- **帧率:** 20fps（setInterval 50ms）

## 二值化算法

1. 获取 ImageData RGBA
2. 灰度 = 0.299×R + 0.587×G + 0.114×B
3. 灰度 > 128 → 亮像素（bit = 1）
4. 按 XBM LSB-first 打包为 1024 字节

## 视觉风格

- 背景: #121212
- 强调色: #00ffcc
- 次要背景: #1e1e1e
- 字体: 等宽 'Courier New', monospace
- 区块: 圆角卡片式，极简边框

## 边界处理

- 视频未加载时禁用推流按钮
- WebSocket 断开时自动停止推流
- 视频 loop 属性避免播放中断
- 页面卸载时自动关闭 WebSocket
