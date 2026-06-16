# WebServerManager 重定向路由器重构

**日期：** 2026-06-16
**状态：** 已批准

## 背景

当前 WebServerManager 从 LittleFS 读取 `index.html` 并通过 HTTP 80 端口托管前端控制台页面。前端页面将迁移至 GitHub Pages（`kloms-fame.github.io/BitStream_12864/`），ESP8266 仅作为重定向路由器。

## 设计

### 行为变更

- **之前：** `GET /` → 从 LittleFS 读取 `index.html`，流式返回 HTML
- **之后：** `GET /` → 构造 `https://kloms-fame.github.io/BitStream_12864/?ip=<局域网IP>`，返回 HTTP 302 重定向

### 影响范围

| 文件 | 变更 |
|---|---|
| `include/WebServerManager.h` | 移除 `#include <LittleFS.h>`，更新类/方法文档注释 |
| `src/WebServerManager.cpp` | 移除 LittleFS 初始化与文件读取，替换为 URL 构造 + 302 重定向 |
| `src/main.cpp` | 无需修改 |

### 核心逻辑

```cpp
void WebServerManager::begin()
{
    server.on("/", HTTP_GET, [this]()
    {
        String url = "https://kloms-fame.github.io/BitStream_12864/?ip="
                   + WiFi.localIP().toString();
        server.sendHeader("Location", url, true);
        server.send(302, "text/plain", "");
    });

    server.begin();
    Serial.println("HTTP redirect server started on port 80");
}
```

### 不变项

- 端口：80
- 类接口：`begin()` / `loop()` 签名不变
- `loop()` 实现不变（`server.handleClient()`）
- `main.cpp` 调用方式不变

## 优势

- **Flash 节省：** 不再需要 LittleFS 分区存储 HTML 文件
- **维护性：** 前端更新只需推送 GitHub Pages，无需重新烧录固件
- **简洁性：** WebServerManager 从 ~60 行缩减至 ~30 行
