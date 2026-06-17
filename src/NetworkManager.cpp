/**
 * @file    NetworkManager.cpp
 * @brief   NetworkManager — AP/STA 双状态机 + WebSocket ACK 锁步完整实现
 *
 * @details 启动序列：
 *          LittleFS 挂载 → /wifi.txt 加载 → STA 3次重试 → 失败则 AP
 *
 *          WebSocket 协议：
 *          PC → ESP   BIN  1024B XBM 帧
 *          ESP → PC   TXT  "ACK"
 *          PC → ESP   TXT  "BRIGHTNESS:0~255"
 *          ESP → PC   TXT  "OK"
 */

#include "NetworkManager.h"

/* ========================================================================== */
/*  静态成员初始化                                                             */
/* ========================================================================== */

NetworkManager::Mode NetworkManager::s_mode = NetworkManager::Mode::INIT;

const char* NetworkManager::CRED_FILE = "/wifi.txt";
const char* NetworkManager::AP_SSID   = "OLED-BitStream";

/* ========================================================================== */
/*  构造函数                                                                  */
/* ========================================================================== */

NetworkManager::NetworkManager()
    : m_webSocket(WS_PORT)
{
}

/* ========================================================================== */
/*  begin() — 入口：凭据加载 → STA / AP → WebSocket                            */
/* ========================================================================== */

void NetworkManager::begin(FrameCallback onFrame, BrightnessCallback onBrightness)
{
    m_onFrame      = onFrame;
    m_onBrightness = onBrightness;

    /* ---- 1. 挂载 LittleFS ------------------------------------------------- */
    Serial.println(F("[NET] 挂载 LittleFS ..."));
    if (!LittleFS.begin())
    {
        Serial.println(F("[NET] 挂载失败，尝试格式化 ..."));
        if (!LittleFS.format() || !LittleFS.begin())
        {
            Serial.println(F("[NET] 文件系统不可用，直接进入 AP 模式"));
            startAPMode();
            startWebSocket();
            return;
        }
    }
    Serial.println(F("  [OK]"));

    /* ---- 2. 加载凭据 ----------------------------------------------------- */
    String savedSSID, savedPass;
    const bool hasCreds = loadCredentials(savedSSID, savedPass);

    if (hasCreds && savedSSID.length() > 0)
    {
        Serial.printf("[NET] 找到凭据: SSID=\"%s\"\n", savedSSID.c_str());

        if (connectWiFi(savedSSID.c_str(), savedPass.c_str()))
        {
            s_mode = Mode::STATION_CONNECTED;
        }
        else
        {
            Serial.println(F("[NET] 3次连接均失败，清除旧凭据，进入 AP"));
            LittleFS.remove(CRED_FILE);
            startAPMode();
        }
    }
    else
    {
        Serial.println(F("[NET] 无已保存凭据，进入 AP 模式"));
        startAPMode();
    }

    /* ---- 3. 启动 WebSocket（双模式均运行）--------------------------------- */
    startWebSocket();
}

/* ========================================================================== */
/*  loop()                                                                     */
/* ========================================================================== */

void NetworkManager::loop()
{
    m_webSocket.loop();
}

/* ========================================================================== */
/*  isAPMode()                                                                 */
/* ========================================================================== */

bool NetworkManager::isAPMode()
{
    return s_mode == Mode::AP_CONFIG;
}

/* ========================================================================== */
/*  saveWiFiAndReboot()                                                        */
/* ========================================================================== */

void NetworkManager::saveWiFiAndReboot(const char *ssid, const char *pass)
{
    Serial.printf("[NET] 保存凭据: SSID=\"%s\"\n", ssid);

    File f = LittleFS.open(CRED_FILE, "w");
    if (f)
    {
        f.print(ssid);
        f.print('\n');
        f.print(pass);
        f.close();
        Serial.printf("[NET] 凭据已写入 %s\n", CRED_FILE);
    }
    else
    {
        Serial.println(F("[NET] 无法创建凭据文件"));
    }

    Serial.println(F("[NET] 500ms 后重启 ..."));
    delay(500);
    ESP.restart();
}

/* ========================================================================== */
/*  getClientCount()                                                           */
/* ========================================================================== */

uint8_t NetworkManager::getClientCount()
{
    return m_webSocket.connectedClients();
}

/* ========================================================================== */
/*  私有 — loadCredentials()                                                   */
/* ========================================================================== */

bool NetworkManager::loadCredentials(String &ssid, String &pass)
{
    if (!LittleFS.exists(CRED_FILE))
        return false;

    File f = LittleFS.open(CRED_FILE, "r");
    if (!f)
    {
        Serial.println(F("[NET] 无法打开凭据文件"));
        return false;
    }

    // 格式: ssid\npassword
    ssid = f.readStringUntil('\n');
    pass = f.readStringUntil('\n');
    f.close();

    ssid.trim();
    pass.trim();

    if (ssid.length() == 0)
    {
        Serial.println(F("[NET] 凭据文件中 SSID 为空"));
        return false;
    }

    return true;
}

/* ========================================================================== */
/*  私有 — connectWiFi() — 3次重试 × 15s超时 × 1s间隔                        */
/* ========================================================================== */

bool NetworkManager::connectWiFi(const char *ssid, const char *pass)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    for (uint8_t attempt = 0; attempt < MAX_RETRIES; ++attempt)
    {
        Serial.printf("[NET] 连接 WiFi (第 %u/%u 次): \"%s\" ...\n",
                      attempt + 1, MAX_RETRIES, ssid);

        const unsigned long deadline = millis() + 15000UL;

        while (WiFi.status() != WL_CONNECTED)
        {
            if (millis() > deadline)
            {
                Serial.printf("[NET] 第 %u 次连接超时 (15s)\n", attempt + 1);
                break;
            }
            delay(200);
            Serial.print('.');
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println();
            Serial.printf("[NET] WiFi 连接成功! IP: %s, RSSI: %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());

            // 回写凭据确保最新正确
            File f = LittleFS.open(CRED_FILE, "w");
            if (f)
            {
                f.print(ssid);
                f.print('\n');
                f.print(pass);
                f.close();
            }

            return true;
        }

        if (attempt < MAX_RETRIES - 1)
        {
            Serial.println(F("[NET] 等待 1 秒后重试 ..."));
            delay(1000);
        }
    }

    Serial.println(F("[NET] 全部 3 次重试均失败"));
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    return false;
}

/* ========================================================================== */
/*  私有 — startAPMode()                                                       */
/* ========================================================================== */

void NetworkManager::startAPMode()
{
    Serial.println(F("[NET] >>> 进入 AP 模式 <<<"));

    WiFi.mode(WIFI_AP);

    // 配置静态 IP: 192.168.4.1 / 255.255.255.0
    const IPAddress apIP(192, 168, 4, 1);
    const IPAddress subnet(255, 255, 255, 0);

    WiFi.softAPConfig(apIP, apIP, subnet);
    WiFi.softAP(AP_SSID, nullptr, 1, 0, 4);  // 无密码，信道 1，隐藏关闭，最大 4 客户端

    Serial.printf("[NET] AP 热点已开放: SSID=\"%s\" (无密码)\n", AP_SSID);
    Serial.printf("[NET] IP: %s\n", apIP.toString().c_str());

    s_mode = Mode::AP_CONFIG;
}

/* ========================================================================== */
/*  私有 — startWebSocket() — 注册双回调 + ACK 锁步                            */
/* ========================================================================== */

void NetworkManager::startWebSocket()
{
    m_webSocket.begin();
    Serial.printf("[NET] WebSocket 服务已启动 — 端口 %u\n", WS_PORT);

    m_webSocket.onEvent(
        [this](uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length)
        {
            switch (type)
            {
            /* ---------------------------------------------------------- */
            /*  客户端连接                                                   */
            /* ---------------------------------------------------------- */
            case WStype_CONNECTED:
            {
                const IPAddress remoteIP = m_webSocket.remoteIP(clientNum);
                Serial.printf("[NET] 客户端 #%u 已连接 — IP: %s\n",
                              clientNum, remoteIP.toString().c_str());
                break;
            }

            /* ---------------------------------------------------------- */
            /*  客户端断开                                                   */
            /* ---------------------------------------------------------- */
            case WStype_DISCONNECTED:
            {
                Serial.printf("[NET] 客户端 #%u 已断开\n", clientNum);
                break;
            }

            /* ---------------------------------------------------------- */
            /*  二进制帧 — 推流数据入口                                        */
            /* ---------------------------------------------------------- */
            case WStype_BIN:
            {
                if (length == FRAME_SIZE)
                {
                    if (m_onFrame)
                    {
                        m_onFrame(payload, length);
                    }
                    m_webSocket.sendTXT(clientNum, "ACK");
                }
                else
                {
                    Serial.printf("[NET] 客户端 #%u 异常 BIN 帧: "
                                  "期望 %u 字节，实际 %u 字节\n",
                                  clientNum, FRAME_SIZE, length);
                }
                break;
            }

            /* ---------------------------------------------------------- */
            /*  文本帧 — 控制指令入口                                          */
            /* ---------------------------------------------------------- */
            case WStype_TEXT:
            {
                const char *text = reinterpret_cast<const char *>(payload);
                uint8_t brightness = 0;

                if (parseBrightnessCommand(text, brightness))
                {
                    Serial.printf("[NET] 客户端 #%u 指令: 亮度 → %u\n",
                                  clientNum, brightness);

                    if (m_onBrightness)
                    {
                        m_onBrightness(brightness);
                    }
                    m_webSocket.sendTXT(clientNum, "OK");
                }
                else
                {
                    Serial.printf("[NET] 客户端 #%u 未知指令: \"%s\"\n",
                                  clientNum, text);
                }
                break;
            }

            /* ---------------------------------------------------------- */
            /*  PING/PONG/ERROR/FRAGMENT — 全事件日志                        */
            /* ---------------------------------------------------------- */
            default:
            {
                const char *typeName = "UNKNOWN";
                switch (type)
                {
                case WStype_ERROR:                typeName = "ERROR";              break;
                case WStype_PING:                 typeName = "PING";               break;
                case WStype_PONG:                 typeName = "PONG";               break;
                case WStype_FRAGMENT_BIN_START:   typeName = "FRAGMENT_BIN_START"; break;
                case WStype_FRAGMENT_TEXT_START:  typeName = "FRAGMENT_TEXT_START";break;
                case WStype_FRAGMENT:             typeName = "FRAGMENT";           break;
                case WStype_FRAGMENT_FIN:         typeName = "FRAGMENT_FIN";       break;
                default: break;
                }
                Serial.printf("[NET] 客户端 #%u WS事件: %s (type=%d, len=%u)\n",
                              clientNum, typeName, static_cast<int>(type), length);
                if (type == WStype_ERROR && payload && length > 0)
                {
                    Serial.printf("[NET] 客户端 #%u WS错误payload: %.*s\n",
                                  clientNum, static_cast<int>(length), payload);
                }
                break;
            }
            }
        });
}

/* ========================================================================== */
/*  私有静态 — parseBrightnessCommand()                                        */
/* ========================================================================== */

bool NetworkManager::parseBrightnessCommand(const char *text, uint8_t &brightness)
{
    const char prefix[] = "BRIGHTNESS:";
    constexpr size_t prefixLen = sizeof(prefix) - 1;

    for (size_t i = 0; i < prefixLen; ++i)
    {
        char c = text[i];
        if (c == '\0') return false;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c != prefix[i]) return false;
    }

    const char *numStr = text + prefixLen;
    if (*numStr == '\0') return false;

    uint16_t value = 0;
    for (const char *p = numStr; *p != '\0'; ++p)
    {
        if (*p < '0' || *p > '9') return false;
        value = value * 10 + (*p - '0');
        if (value > 255) return false;
    }

    brightness = static_cast<uint8_t>(value);
    return true;
}
