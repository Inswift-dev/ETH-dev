#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ETH.h>
#include <HTTPClient.h>

// #include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <CCTools.h>

#include "config.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "zb.h"
#include "mqtt.h"
#include "const/keys.h"
#include "main.h"

extern struct SysVarsStruct vars;
extern struct ThisConfigStruct hwConfig;
extern BrdConfigStruct brdConfigs[BOARD_CFG_CNT];

extern struct SystemConfigStruct systemCfg;
extern struct NetworkConfigStruct networkCfg;
extern struct VpnConfigStruct vpnCfg;
extern struct MqttConfigStruct mqttCfg;

extern LEDControl ledControl;

extern const char *tempFile;

size_t lastSize = 0;

String tag_ZB = "[RCP]";

extern CCTools CCTool;

bool zbFwCheck()
{
    const int maxAttempts = 3;
    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        LOGD("Try: %d", attempt + 1);
        delay(500 * (attempt * 2));
        if (CCTool.checkFirmwareVersion())
        {
            printLogMsg(tag_ZB + " FW: " + String(CCTool.chip.fwRev));
            // if (systemCfg.zbRole == UNDEFINED)
            // {
            //     systemCfg.zbRole = COORDINATOR;
            //     saveSystemConfig(systemCfg);
            // }
            return true;
        }
        else
        {
            CCTool.restart();
        }
    }
    printLogMsg(tag_ZB + " FW: Unknown! Check serial speed!");
    return false;
}

void zbHwCheck()
{
    // ledControl.modeLED.mode = LED_BLINK_1Hz;
    if (CCTool.detectChipInfo())
    {
        printLogMsg(tag_ZB + " Chip: " + CCTool.chip.hwRev);
        printLogMsg(tag_ZB + " IEEE: " + CCTool.chip.ieee);
        LOGI("modeCfg %s", String((CCTool.chip.modeCfg), HEX).c_str());
        LOGI("bslCfg %s", String((CCTool.chip.bslCfg), HEX).c_str());
        printLogMsg(tag_ZB + " Flash size: " + String(CCTool.chip.flashSize / 1024) + " KB");

        vars.hwZigbeeIs = true;
        // ledControl.modeLED.mode = LED_OFF;
    }
    else
    {
        printLogMsg(tag_ZB + " No Zigbee chip!");
        vars.hwZigbeeIs = false;
        ledControl.modeLED.mode = LED_OFF;   
        ledControl.powerLED.mode = LED_OFF;   
        ledControl.statusLED.mode = LED_BLINK_1Hz;       
    }
    CCTool.restart();
}

bool zbLedToggle()
{
    if (CCTool.ledToggle())
    {
        if (CCTool.ledState == 1)
        {
            printLogMsg("[RCP] LED toggle ON");
            // vars.zbLedState = 1;
        }
        else
        {
            printLogMsg("[RCP] LED toggle OFF");
            // vars.zbLedState = 0;
        }
        return true;
    }
    else
    {
        printLogMsg("[RCP] LED toggle ERROR");
        return false;
    }
}

bool zigbeeErase()
{
    if (CCTool.eraseFlash())
    {
        LOGD("magic");
        return true;
    }
    return false;
}
void nvPrgs(const String &inputMsg)
{

    const uint8_t eventLen = 30;
    String msg = inputMsg;
    if (msg.length() > 25)
    {
        msg = msg.substring(0, 25);
    }
    sendEvent(tagZB_NV_prgs, eventLen, msg);
    LOGD("%s", msg.c_str());
}

void zbEraseNV(void *pvParameters)
{
    vars.zbFlashing = true;
    CCTool.nvram_reset(nvPrgs);
    logClear();
    printLogMsg("NVRAM erase finish! Restart CC2652!");
    vars.zbFlashing = false;
    vTaskDelete(NULL);
}

void flashZbUrl(String url)
{
    // zbFwCheck();
    ledControl.modeLED.mode = LED_BLINK_3Hz;
    vars.zbFlashing = true;

    // checkDNS();
    //delay(250);

    Serial2.updateBaudRate(500000);

    freeHeapPrint();
    delay(250);
    
    // 条件性停止 WiFi：
    // 如果以太网和 WiFi 都连接时，断开 WiFi 以释放内存
    // 如果只有 WiFi 或只有以太网其中一个连接，就保持连接
    if (networkCfg.wifiEnable)
    {
        if (vars.connectedEther)
        {
            // 以太网和 WiFi 都连接，停止 WiFi 以释放内存
            LOGD("Both Ethernet and WiFi connected, stopping WiFi to free memory");
            stopWifi();
        }
        else
        {
            // 只有 WiFi 连接，保持 WiFi 运行以便 Web 服务器和 HTTPClient 正常工作
            LOGD("Only WiFi connected, keeping WiFi for web server and HTTPClient");
        }
    }
    
    if (mqttCfg.enable)
    {
        mqttDisconnectCleanup();
    }
    
    // 清理内存
    heap_caps_free(NULL);
    
    freeHeapPrint();
    delay(250);

    float last_percent = 0;

    const uint8_t eventLen = 11;
    // 使用静态缓冲区存储进度字符串，避免频繁创建 String 对象
    static char percentStr[16];

    auto progressShow = [&last_percent](float percent) mutable
    {
        // 每 5% 更新一次，减少 sendEvent 调用频率
        if ((percent - last_percent) > 5.0 || percent < 0.1 || percent == 100)
        {
            LOGI("%.2f%%", percent);
            // 使用 snprintf 而不是 String 构造函数，减少内存分配
            snprintf(percentStr, sizeof(percentStr), "%.2f", percent);
            sendEvent(tagZB_FW_prgs, eventLen, String(percentStr));
            last_percent = percent;
        }
    };

    printLogMsg("Start Zigbee flashing");
    sendEvent(tagZB_FW_info, eventLen, String("start"));

    // https://raw.githubusercontent.com/Inswift-dev/inswift-eth/main/ti-cc2652/coordinator/CC1352P7_coordinator_20240316.bin
    //  CCTool.enterBSL();
    int key = url.indexOf("?b=");

    String clear_url = url.substring(0, key);

    // printLogMsg("Clear from " + clear_url);
    String baud_str = url.substring(key + 3, url.length());
    // printLogMsg("Baud " + baud_str);
    systemCfg.serialSpeed = baud_str.toInt();

    // 优化日志输出，减少 String 拼接
    char logMsg[256];
    snprintf(logMsg, sizeof(logMsg), "ZB flash %s @ %d", clear_url.c_str(), systemCfg.serialSpeed);
    printLogMsg(logMsg);

    sendEvent(tagZB_FW_file, eventLen, String(clear_url));

    if (eraseWriteZbUrl(clear_url.c_str(), progressShow, CCTool))
    {
        sendEvent(tagZB_FW_info, eventLen, String("finish"));
        printLogMsg("Flashed successfully");
        Serial2.updateBaudRate(systemCfg.serialSpeed);

        int lineIndex = clear_url.lastIndexOf("_");
        int binIndex = clear_url.lastIndexOf(".bin");
        int lastSlashIndex = clear_url.lastIndexOf("/");

        if (lineIndex > -1 && binIndex > -1 && lastSlashIndex > -1)
        {
            String zbFw = clear_url.substring(lineIndex + 1, binIndex);
            // LOGI("1 %s", zbFw);

            strncpy(systemCfg.zbFw, zbFw.c_str(), sizeof(systemCfg.zbFw) - 1);
            zbFw = "";

            int i = 0;

            int preLastSlash = -1;
            // LOGI("2 %s", String(url.c_str()));

            while (clear_url.indexOf("/", i) > -1 && i < lastSlashIndex)
            {
                int result = clear_url.indexOf("/", i);
                // LOGI("r %d", result);
                if (result > -1)
                {
                    i = result + 1;
                    if (result < lastSlashIndex)
                    {
                        preLastSlash = result;
                        // LOGI("pl %d", preLastSlash);
                    }
                }
                // LOGI("l %d", lastSlashIndex);
                // delay(500);
            }

            // LOGD("%s %s", String(preLastSlash), String(lastSlashIndex));
            String zbRole = clear_url.substring(preLastSlash + 1, lastSlashIndex);
            LOGI("%s", zbRole.c_str());

            if (zbRole.indexOf("coordinator") > -1)
            {
                systemCfg.zbRole = COORDINATOR;
            }
            else if (zbRole.indexOf("router") > -1)
            {
                systemCfg.zbRole = ROUTER;
            }
            else if (zbRole.indexOf("thread") > -1)
            {
                systemCfg.zbRole = OPENTHREAD;
            }
            else
            {
                systemCfg.zbRole = UNDEFINED;
            }
            zbRole = "";
            // systemCfg.zbSaveinfo = false;
            saveSystemConfig(systemCfg);
        }
        else
        {
            LOGW("URL error");
        }
        // if (systemCfg.zbRole == COORDINATOR)
        // {
        //     zbFwCheck();
        //     // zbLedToggle();
        //     // delay(1000);
        //     // zbLedToggle();
        // }
        sendEvent(tagZB_FW_file, eventLen, String(systemCfg.zbFw));
        delay(500);
        restartDevice();
    }
    else
    {
        Serial2.updateBaudRate(systemCfg.serialSpeed);
        printLogMsg("Failed to flash Zigbee");
        
        // 发送错误事件
        sendEvent(tagZB_FW_err, eventLen, String("Failed!"));
        
        // 立即处理 Web 服务器，确保事件被发送到前端
        webServerHandleClient();
        delay(100);  // 给一点时间让事件发送
        webServerHandleClient();
    }
    ledControl.modeLED.mode = LED_OFF;
    vars.zbFlashing = false;
    if (mqttCfg.enable && !vars.mqttConn)
    {
        connectToMqtt();
    }
}

/*void printBufferAsHex(const byte *buffer, size_t length)
{
    const char *TAG = "BufferHex";
    char hexStr[CCTool.TRANSFER_SIZE + 10];
    std::string hexOutput;

    for (size_t i = 0; i < length; ++i)
    {
        if (buffer[i] != 255)
        {
            sprintf(hexStr, "%02X ", buffer[i]);
            hexOutput += hexStr;
        }
    }

    LOGD("Buffer content:\n%s", hexOutput.c_str());
}*/

// 使用静态缓冲区，避免在栈上分配大缓冲区
static byte flashBuffer[CCTools::TRANSFER_SIZE];

bool eraseWriteZbUrl(const char *url, std::function<void(float)> progressShow, CCTools &CCTool)
{
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    // 优化 HTTPClient 配置，减少内存占用
    http.setTimeout(30000);  // 30秒超时
    http.setReuse(false);   // 不重用连接，释放资源

    int loadedSize = 0;
    int totalSize = 0;
    const int maxRetries = 3;  // 从 7 减少到 3，减少重试次数
    int retryCount = 0;
    const int retryDelay = 500;
    bool isSuccess = false;
    bool flashErased = false;  // 标志：是否已擦除 Flash，避免重复擦除

    while (retryCount < maxRetries && !isSuccess)
    {
        // 检查内存，如果内存不足，提前失败
        size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        if (freeHeap < 10000)  // 如果内存少于 10KB，立即失败
        {
            LOGW("Critical low memory during flash: %d bytes, aborting", freeHeap);
            printLogMsg("OTA failed: Insufficient memory");
            sendEvent(tagZB_FW_err, 11, String("Insufficient memory"));
            webServerHandleClient();
            delay(100);
            webServerHandleClient();
            http.end();
            CCTool.restart();
            return false; // 提前返回，不再重试
        }

        if (!dnsLookup(url))
        {
            retryCount++;
            // 如果是早期失败（前 2 次），快速返回
            if (retryCount >= 2)
            {
                LOGW("DNS lookup failed after %d retries, aborting", retryCount);
                printLogMsg("OTA failed: DNS lookup failed");
                sendEvent(tagZB_FW_err, 11, String("DNS lookup failed"));
                webServerHandleClient();
                delay(100);
                webServerHandleClient();
                http.end();
                CCTool.restart();
                return false; // 提前返回
            }
            delay(retryDelay);
            // 确保 Web 服务器可以处理事件，以便前端能及时收到错误信息
            webServerHandleClient();
            continue;
        }

        // 只在第一次或需要重新开始时擦除，避免重复擦除
        if (loadedSize == 0 && !flashErased)
        {
            CCTool.eraseFlash();
            sendEvent("ZB_FW_info", 11, String("erase"));
            printLogMsg("Erase completed!");
            flashErased = true;
        }

        http.begin(url);
        http.addHeader("Content-Type", "application/octet-stream");

        if (loadedSize > 0)
        {
            // 使用字符数组而不是 String，减少内存分配
            char rangeHeader[32];
            snprintf(rangeHeader, sizeof(rangeHeader), "bytes=%d-", loadedSize);
            http.addHeader("Range", rangeHeader);
        }

        int httpCode = http.GET();

        if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_PARTIAL_CONTENT)
        {
            char buffer[100];
            snprintf(buffer, sizeof(buffer), "Failed to download file, HTTP code: %d\n", httpCode);
            printLogMsg(buffer);

            http.end();
            retryCount++;
            
            // 如果是早期失败（前 2 次），快速返回
            if (retryCount >= 2)
            {
                LOGW("HTTP error persists after %d retries, aborting", retryCount);
                printLogMsg("OTA failed: Network error");
                sendEvent(tagZB_FW_err, 11, String("Network error"));
                webServerHandleClient();
                delay(100);
                webServerHandleClient();
                CCTool.restart();
                return false; // 提前返回
            }
            
            delay(retryDelay);
            // 确保 Web 服务器可以处理事件，以便前端能及时收到错误信息
            webServerHandleClient();
            continue;
        }

        if (totalSize == 0)
        {
            totalSize = http.getSize();
        }

        if (loadedSize == 0)
        {
            if (!CCTool.beginFlash(BEGIN_ZB_ADDR, totalSize))
            {
                http.end();
                printLogMsg("Error initializing flash process");
                retryCount++;
                
                // 如果是早期失败（前 2 次），快速返回
                if (retryCount >= 2)
                {
                    LOGW("Flash init failed after %d retries, aborting", retryCount);
                    printLogMsg("OTA failed: Flash init error");
                    sendEvent(tagZB_FW_err, 11, String("Flash init error"));
                    webServerHandleClient();
                    delay(100);
                    webServerHandleClient();
                    CCTool.restart();
                    return false; // 提前返回
                }
                
                delay(retryDelay);
                // 确保 Web 服务器可以处理事件，以便前端能及时收到错误信息
                webServerHandleClient();
                continue;
            }
            printLogMsg("Begin flash");
        }

        // 使用静态缓冲区而不是栈上分配
        WiFiClient *stream = http.getStreamPtr();
        static int callCounter = 0;
        static int webServerCallCounter = 0;

        while (http.connected() && loadedSize < totalSize)
        {
            size_t size = stream->available();
            if (size > 0)
            {
                int c = stream->readBytes(flashBuffer, std::min(size, sizeof(flashBuffer)));
                if (c <= 0)
                {
                    printLogMsg("Failed to read data from stream");
                    break;
                }

                if (!CCTool.processFlash(flashBuffer, c))
                {
                    printLogMsg("Failed to process flash data");
                    loadedSize = 0;
                    retryCount++;
                    
                    // 如果是早期失败（前 2 次），快速返回
                    if (retryCount >= 2)
                    {
                        LOGW("Flash process failed after %d retries, aborting", retryCount);
                        printLogMsg("OTA failed: Flash process error");
                        sendEvent(tagZB_FW_err, 11, String("Flash process error"));
                        webServerHandleClient();
                        delay(100);
                        webServerHandleClient();
                        stream->stop();
                        http.end();
                        CCTool.restart();
                        return false; // 提前返回
                    }
                    
                    delay(retryDelay);
                    // 确保 Web 服务器可以处理事件，以便前端能及时收到错误信息
                    webServerHandleClient();
                    break;
                }

                loadedSize += c;
                float percent = static_cast<float>(loadedSize) / totalSize * 100.0f;
                progressShow(percent);
            }

            esp_task_wdt_reset(); // Reset watchdog
            delay(10); // Yield to the WiFi stack
            
            callCounter++;
            webServerCallCounter++;

            // 更频繁地检查内存（每 100 次循环，约 1 秒）
            if (callCounter % 100 == 0)
            {
                size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
                
                // 如果内存少于 10KB，立即失败
                if (freeHeap < 10000)
                {
                    LOGW("Critical low memory during flash: %d bytes, aborting", freeHeap);
                    printLogMsg("OTA failed: Insufficient memory");
                    sendEvent(tagZB_FW_err, 11, String("Insufficient memory"));
                    webServerHandleClient();
                    delay(100);
                    webServerHandleClient();
                    stream->stop();
                    http.end();
                    CCTool.restart();
                    return false; // 提前返回，不再继续
                }
                else if (freeHeap < 20000)  // 如果内存少于 20KB，记录警告
                {
                    LOGW("Low memory during flash: %d bytes", freeHeap);
                }
                freeHeapPrint();
                callCounter = 0;
            }
            
            // 定期调用 webServerHandleClient，确保 Web 服务器可以处理事件（每 50 次循环，约 0.5 秒）
            if (webServerCallCounter % 50 == 0)
            {
                webServerHandleClient();
                webServerCallCounter = 0;
            }
        }

        stream->stop();
        http.end();

        if (loadedSize >= totalSize)
        {
            isSuccess = true;
        }
    }

    // 如果失败，立即发送错误事件并通知用户
    if (!isSuccess)
    {
        // 发送错误事件，通知用户失败
        printLogMsg("OTA failed after all retries");
        sendEvent(tagZB_FW_err, 11, String("OTA failed"));
        // 确保 Web 服务器可以处理事件，以便前端能及时收到错误信息
        webServerHandleClient();
        delay(100);
        webServerHandleClient();
    }

    http.end();
    CCTool.restart();
    return isSuccess;
}

/*
#include <WiFi.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

// Функция для извлечения хоста из URL


// Функция для получения размера файла
int getFileSize(const char* url) {

    LOGD("URL %s", url);
    const int httpsPort = 443;
    String host = getHostFromUrl(url);

    // Создание сокета
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        Serial.println("Failed to create socket");
        return -1;
    }

    // Настройка адреса сервера
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(httpsPort);

    struct hostent* server = gethostbyname(host.c_str());
    if (server == NULL) {
        Serial.println("Failed to resolve hostname");
        close(sockfd);
        return -1;
    }

    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Установка соединения
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        Serial.println("Connection failed");
        close(sockfd);
        return -1;
    }

    // Отправка HTTP-запроса
    String request = "HEAD " + String(url) + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
        Serial.println("Failed to send request");
        close(sockfd);
        return -1;
    }

    // Чтение ответа
    char buffer[1024];
    int bytes;
    int contentLength = -1;
    while ((bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = 0;
        String response(buffer);

        // Поиск заголовка Content-Length
        int index = response.indexOf("Content-Length: ");
        if (index != -1) {
            int endIndex = response.indexOf("\r\n", index);
            if (endIndex != -1) {
                String lengthStr = response.substring(index + 16, endIndex);
                contentLength = lengthStr.toInt();
                break;
            }
        }
    }

    close(sockfd);
    return contentLength;
}

// Функция для загрузки файла
bool downloadFile(const char* url, std::function<void(float)> progressShow, CCTools &CCTool, int &loadedSize, int &totalSize) {
    const int httpsPort = 443;
    String host = getHostFromUrl(url);

    // Создание сокета
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        Serial.println("Failed to create socket");
        return false;
    }

    // Настройка адреса сервера
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(httpsPort);

    struct hostent* server = gethostbyname(host.c_str());
    if (server == NULL) {
        Serial.println("Failed to resolve hostname");
        close(sockfd);
        return false;
    }

    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Установка соединения
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        Serial.println("Connection failed");
        close(sockfd);
        return false;
    }

    // Отправка HTTP-запроса
    String request = "GET " + String(url) + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n";
    if (loadedSize > 0) {
        request += "Range: bytes=" + String(loadedSize) + "-\r\n";
    }
    request += "\r\n";

    if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
        Serial.println("Failed to send request");
        close(sockfd);
        return false;
    }

    // Чтение ответа
    char buffer[1024];
    int bytes;
    bool headersEnded = false;
    while ((bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = 0;
        String response(buffer);

        if (!headersEnded) {
            int headerEnd = response.indexOf("\r\n\r\n");
            if (headerEnd != -1) {
                headersEnded = true;
                response = response.substring(headerEnd + 4);
            } else {
                continue;
            }
        }

        if (headersEnded) {
            int c = response.length();
            if (!CCTool.processFlash((byte*)response.c_str(), c)) {
                loadedSize = 0;
                close(sockfd);
                return false;
            }
            loadedSize += c;
            float percent = static_cast<float>(loadedSize) / totalSize * 100.0f;
            progressShow(percent);
        }
    }

    close(sockfd);
    return true;
}

bool eraseWriteZbUrl(const char *url, std::function<void(float)> progressShow, CCTools &CCTool)
{
    int loadedSize = 0;
    int totalSize = 0;
    int maxRetries = 7;
    int retryCount = 0;
    int retryDelay = 500;
    bool isSuccess = false;

    while (retryCount < maxRetries && !isSuccess)
    {
        if (loadedSize == 0)
        {
            CCTool.eraseFlash();
            sendEvent("ZB_FW_info", 11, String("erase"));
            printLogMsg("Erase completed!");
        }

        if (loadedSize == 0)
        {
            // Получение размера файла
            totalSize = getFileSize(url);
            if (totalSize <= 0) {
                printLogMsg("Failed to get file size");
                retryCount++;
                delay(retryDelay);
                continue;
            }

            if (!CCTool.beginFlash(BEGIN_ZB_ADDR, totalSize))
            {
                printLogMsg("Error initializing flash process");
                retryCount++;
                delay(retryDelay);
                continue;
            }
            printLogMsg("Begin flash");
        }

        if (downloadFile(url, progressShow, CCTool, loadedSize, totalSize)) {
            isSuccess = true;
        } else {
            retryCount++;
            delay(retryDelay);
        }
    }

    CCTool.restart();
    return isSuccess;
}
*/

#include <FS.h>
#include <LittleFS.h>

String extractChipModelFromFilename(const char *filePath)
{
    String fileName = String(filePath);
    
    // 从路径中提取文件名
    int lastSlash = fileName.lastIndexOf('/');
    if (lastSlash >= 0)
    {
        fileName = fileName.substring(lastSlash + 1);
    }
    
    // 转换为大写以便比较
    fileName.toUpperCase();
    
    // 检查常见的芯片型号模式
    // CC1352P7 和 CC2652P7 都映射到 CC2652P7
    if (fileName.indexOf("CC2652P7") >= 0 || fileName.indexOf("CC1352P7") >= 0)
    {
        return "CC2652P7";
    }
    // CC1352P2 和 CC2652P2 都映射到 CC2652P2
    else if (fileName.indexOf("CC2652P2") >= 0 || fileName.indexOf("CC1352P2") >= 0)
    {
        return "CC2652P2";
    }
    else if (fileName.indexOf("CC2652RB") >= 0)
    {
        return "CC2652RB";
    }
    else if (fileName.indexOf("CC2652") >= 0 || fileName.indexOf("CC1352") >= 0)
    {
        // 如果只有CC2652或CC1352，尝试更精确的匹配
        // 检查是否有P2或P7的明确标识
        if (fileName.indexOf("P2") >= 0)
        {
            return "CC2652P2";
        }
        else if (fileName.indexOf("P7") >= 0)
        {
            return "CC2652P7";
        }
        // 如果没有明确标识，返回通用CC2652
        return "CC2652";
    }
    
    // 如果无法从文件名提取，返回空字符串
    return "";
}

bool validateChipModel(const char *filePath, const char *currentChipModel)
{
    String firmwareChipModel = extractChipModelFromFilename(filePath);
    
    // 检查是否无法获取当前芯片型号
    bool currentChipUnknown = (!currentChipModel || strlen(currentChipModel) == 0);
    
    // 情况1：如果无法检测芯片型号，但固件文件名明确，允许升级
    if (currentChipUnknown && firmwareChipModel.length() > 0)
    {
        LOGW("Cannot detect current chip model, but firmware filename contains chip model: %s, allowing upgrade", firmwareChipModel.c_str());
        return true;
    }
    
    // 情况2：如果固件文件名也不包含芯片型号，不允许升级
    if (firmwareChipModel.length() == 0)
    {
        if (currentChipUnknown)
        {
            LOGW("Cannot detect current chip model and cannot extract chip model from filename, blocking upgrade");
        }
        else
        {
            LOGW("Cannot extract chip model from filename, blocking upgrade");
        }
        return false;
    }
    
    // 情况3：如果两者都有值，进行严格的匹配校验
    String currentChip = String(currentChipModel);
    currentChip.toUpperCase();
    
    // 精确匹配（主要检查）
    if (firmwareChipModel == currentChip)
    {
        return true;
    }
    
    // 处理 CC2652P2 的变体（launchpad/other）
    // 当前芯片是 CC2652P2 的变体，固件是 CC2652P2
    if (currentChip.indexOf("CC2652P2") >= 0 && firmwareChipModel == "CC2652P2")
    {
        return true;
    }
    // 当前芯片是 CC2652P2，固件是 CC2652P2 的变体
    if (currentChip == "CC2652P2" && firmwareChipModel.indexOf("CC2652P2") >= 0)
    {
        return true;
    }
    
    // 处理 CC2652P7 的变体（如果有的话）
    if (currentChip.indexOf("CC2652P7") >= 0 && firmwareChipModel == "CC2652P7")
    {
        return true;
    }
    if (currentChip == "CC2652P7" && firmwareChipModel.indexOf("CC2652P7") >= 0)
    {
        return true;
    }
    
    // 通用 CC2652 固件可以用于 P2 或 P7（向后兼容）
    // 但 P2 和 P7 的固件不能互换
    if (firmwareChipModel == "CC2652" && (currentChip.indexOf("CC2652P2") >= 0 || currentChip.indexOf("CC2652P7") >= 0))
    {
        LOGW("Firmware chip model is generic CC2652, allowing upgrade");
        return true;
    }
    
    // 其他情况都不允许（包括 P2 和 P7 互相升级）
    return false;
}

bool eraseWriteZbFile(const char *filePath, std::function<void(float)> progressShow, CCTools &CCTool)
{
    File file = LittleFS.open(filePath, "r");
    if (!file)
    {
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "Failed to open file: %s", filePath);
        printLogMsg(buffer);
        return false;
    }

    int totalSize = file.size();
    if (totalSize <= 0)
    {
        printLogMsg("Invalid file size");
        file.close();
        return false;
    }

    // 擦除Flash
    if (!CCTool.eraseFlash())
    {
        printLogMsg("Failed to erase flash");
        file.close();
        return false;
    }
    sendEvent(tagZB_FW_info, 11, String("erase"));
    printLogMsg("Erase completed!");

    // 初始化Flash写入
    if (!CCTool.beginFlash(BEGIN_ZB_ADDR, totalSize))
    {
        printLogMsg("Error initializing flash process");
        file.close();
        return false;
    }
    printLogMsg("Begin flash");

    // 使用静态缓冲区，避免在栈上分配大缓冲区
    static byte flashBuffer[CCTools::TRANSFER_SIZE];
    int loadedSize = 0;
    static int callCounter = 0;
    static int webServerCallCounter = 0;

    while (file.available() && loadedSize < totalSize)
    {
        // 检查内存
        size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        if (freeHeap < 10000)
        {
            LOGW("Critical low memory during flash: %d bytes, aborting", freeHeap);
            printLogMsg("Flash failed: Insufficient memory");
            sendEvent(tagZB_FW_err, 11, String("Insufficient memory"));
            webServerHandleClient();
            delay(100);
            webServerHandleClient();
            file.close();
            CCTool.restart();
            return false;
        }

        size_t size = file.available();
        int c = file.readBytes(reinterpret_cast<char *>(flashBuffer), std::min(size, sizeof(flashBuffer)));
        
        if (c <= 0)
        {
            printLogMsg("Failed to read data from file");
            break;
        }

        if (!CCTool.processFlash(flashBuffer, c))
        {
            printLogMsg("Failed to process flash data");
            file.close();
            CCTool.restart();
            return false;
        }

        loadedSize += c;
        float percent = static_cast<float>(loadedSize) / totalSize * 100.0f;
        progressShow(percent);

        esp_task_wdt_reset(); // Reset watchdog
        delay(10); // Yield to allow other processes

        callCounter++;
        webServerCallCounter++;

        // 定期检查内存（每100次循环）
        if (callCounter % 100 == 0)
        {
            freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            if (freeHeap < 10000)
            {
                LOGW("Critical low memory during flash: %d bytes, aborting", freeHeap);
                printLogMsg("Flash failed: Insufficient memory");
                sendEvent(tagZB_FW_err, 11, String("Insufficient memory"));
                webServerHandleClient();
                delay(100);
                webServerHandleClient();
                file.close();
                CCTool.restart();
                return false;
            }
            callCounter = 0;
        }

        // 定期调用webServerHandleClient（每50次循环）
        if (webServerCallCounter % 50 == 0)
        {
            webServerHandleClient();
            webServerCallCounter = 0;
        }
    }

    file.close();
    CCTool.restart();
    
    if (loadedSize >= totalSize)
    {
        return true;
    }
    else
    {
        printLogMsg("Flash incomplete");
        return false;
    }
}

void flashZbFile(const char *filePath, const char *originalFilename)
{
    // 校验芯片型号 - 使用原始文件名进行校验
    const char *filenameForValidation = (originalFilename && strlen(originalFilename) > 0) ? originalFilename : filePath;
    const char *currentChipModel = CCTool.chip.hwRev.c_str();
    if (strlen(currentChipModel) == 0)
    {
        // 如果当前芯片型号未知，使用保存的型号
        currentChipModel = systemCfg.zbHw;
    }
    
    LOGD("Validating chip model - filePath: %s, originalFilename: %s, currentChip: %s", 
         filePath, originalFilename ? originalFilename : "null", currentChipModel);
    
    if (!validateChipModel(filenameForValidation, currentChipModel))
    {
        String firmwareChipModel = extractChipModelFromFilename(filenameForValidation);
        char errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), 
                 "Chip model mismatch! Current: %s, Firmware: %s", 
                 currentChipModel, firmwareChipModel.c_str());
        
        printLogMsg(errorMsg);
        LOGD("Chip model validation failed: %s", errorMsg);
        
        // 先发送错误状态信息，让前端知道升级失败
        sendEvent(tagZB_FW_info, 11, String("error"));
        webServerHandleClient();
        delay(50);
        
        // 发送格式化的错误消息，前端可以解析显示
        String errorEvent = String("Chip model mismatch: Current=") + String(currentChipModel) + 
                          String(", Firmware=") + firmwareChipModel;
        sendEvent(tagZB_FW_err, 11, errorEvent);
        
        // 多次处理 Web 服务器，确保事件被发送到前端
        for (int i = 0; i < 10; i++)
        {
            webServerHandleClient();
            delay(50);
        }
        
        vars.zbFlashing = false;
        ledControl.modeLED.mode = LED_OFF;
        return;
    }
    
    ledControl.modeLED.mode = LED_BLINK_3Hz;
    vars.zbFlashing = true;

    Serial2.updateBaudRate(500000);

    freeHeapPrint();
    delay(250);
    
    // 条件性停止 WiFi
    if (networkCfg.wifiEnable)
    {
        if (vars.connectedEther)
        {
            LOGD("Both Ethernet and WiFi connected, stopping WiFi to free memory");
            stopWifi();
        }
        else
        {
            LOGD("Only WiFi connected, keeping WiFi for web server");
        }
    }
    
    if (mqttCfg.enable)
    {
        mqttDisconnectCleanup();
    }
    
    // 清理内存
    heap_caps_free(NULL);
    
    freeHeapPrint();
    delay(250);

    float last_percent = 0;

    const uint8_t eventLen = 11;
    static char percentStr[16];

    auto progressShow = [&last_percent](float percent) mutable
    {
        // 每 5% 更新一次，减少 sendEvent 调用频率
        if ((percent - last_percent) > 5.0 || percent < 0.1 || percent == 100)
        {
            LOGI("%.2f%%", percent);
            snprintf(percentStr, sizeof(percentStr), "%.2f", percent);
            sendEvent(tagZB_FW_prgs, eventLen, String(percentStr));
            last_percent = percent;
        }
    };

    printLogMsg("Start Zigbee flashing from file");
    sendEvent(tagZB_FW_info, eventLen, String("start"));

    char logMsg[256];
    snprintf(logMsg, sizeof(logMsg), "ZB flash from file: %s", filePath);
    printLogMsg(logMsg);

    sendEvent(tagZB_FW_file, eventLen, String(filePath));

    if (eraseWriteZbFile(filePath, progressShow, CCTool))
    {
        sendEvent(tagZB_FW_info, eventLen, String("finish"));
        printLogMsg("Flashed successfully");
        
        // 从文件名提取设备类型和设置波特率
        String filenameForExtraction = (originalFilename && strlen(originalFilename) > 0) ? String(originalFilename) : String(filePath);
        String filenameUpper = filenameForExtraction;
        filenameUpper.toUpperCase();  // 转换为大写以便不区分大小写匹配
        
        // 提取设备类型（兼容大小写）
        bool deviceTypeDetected = false;
        
        if (filenameUpper.indexOf("COORDINATOR") >= 0)
        {
            systemCfg.zbRole = COORDINATOR;
            systemCfg.serialSpeed = 115200;  // coordinator默认115200
            deviceTypeDetected = true;
            LOGD("Detected device type: COORDINATOR, baud rate: 115200");
        }
        else if (filenameUpper.indexOf("ROUTER") >= 0)
        {
            systemCfg.zbRole = ROUTER;
            systemCfg.serialSpeed = 115200;  // router默认115200
            deviceTypeDetected = true;
            LOGD("Detected device type: ROUTER, baud rate: 115200");
        }
        else if (filenameUpper.indexOf("THREAD") >= 0 || filenameUpper.indexOf("_RCP_") >= 0 || filenameUpper.indexOf("_RCP.") >= 0)
        {
            // thread和rcp都映射到OPENTHREAD，波特率为460800
            systemCfg.zbRole = OPENTHREAD;
            systemCfg.serialSpeed = 460800;  // thread/rcp波特率为460800
            deviceTypeDetected = true;
            LOGD("Detected device type: OPENTHREAD (thread/rcp), baud rate: 460800");
        }
        
        if (!deviceTypeDetected)
        {
            // 如果无法识别设备类型，保持当前设置
            LOGW("Cannot detect device type from filename: %s, keeping current settings", filenameForExtraction.c_str());
        }
        
        Serial2.updateBaudRate(systemCfg.serialSpeed);

        // 尝试从文件名提取固件版本信息（最后一段的日期）
        // 使用 originalFilename 而不是 filePath，因为 filePath 是固定路径 /fw.hex
        // const char *filenameForExtraction = (originalFilename && strlen(originalFilename) > 0) ? originalFilename : filePath;
        String filePathStr = filenameForExtraction;
        int lineIndex = filePathStr.lastIndexOf("_");
        int binIndex = filePathStr.lastIndexOf(".bin");
        int hexIndex = filePathStr.lastIndexOf(".hex");
        int extIndex = (binIndex > hexIndex) ? binIndex : hexIndex;

        if (lineIndex > -1 && extIndex > -1 && extIndex > lineIndex)
        {
            String zbFw = filePathStr.substring(lineIndex + 1, extIndex);
            
            // 验证是否为日期格式（8位数字，如 20250403）
            bool isValidDate = (zbFw.length() == 8);
            if (isValidDate)
            {
                for (int i = 0; i < 8; i++)
                {
                    if (!isdigit(zbFw.charAt(i)))
                    {
                        isValidDate = false;
                        break;
                    }
                }
            }
            
            if (isValidDate)
            {
                strncpy(systemCfg.zbFw, zbFw.c_str(), sizeof(systemCfg.zbFw) - 1);
                systemCfg.zbFw[sizeof(systemCfg.zbFw) - 1] = '\0';
                LOGW("Extracted firmware version from filename: %s", systemCfg.zbFw);
            }
            else
            {
                // 如果不是日期格式，版本为空
                systemCfg.zbFw[0] = '\0';
                LOGW("No valid date found in filename, version set to empty");
            }
        }
        else
        {
            // 如果无法从文件名提取，版本为空
            systemCfg.zbFw[0] = '\0';
            LOGW("Cannot extract version from filename, version set to empty");
        }

        // 保存配置（包括设备类型和波特率）
        // systemCfg.zbSaveinfo = false;
        saveSystemConfig(systemCfg);

        // if (systemCfg.zbRole == COORDINATOR)
        // {
        //     zbFwCheck();
        // }
        sendEvent(tagZB_FW_file, eventLen, String(systemCfg.zbFw));
        
        // 删除固件文件以释放文件系统空间
        if (LittleFS.exists(filePath))
        {
            if (LittleFS.remove(filePath))
            {
                LOGD("Firmware file deleted: %s", filePath);
                printLogMsg("Firmware file deleted");
            }
            else
            {
                LOGW("Failed to delete firmware file: %s", filePath);
            }
        }
        
        delay(500);
        restartDevice();
    }
    else
    {
        Serial2.updateBaudRate(systemCfg.serialSpeed);
        printLogMsg("Failed to flash Zigbee");
        
        // 发送错误事件
        sendEvent(tagZB_FW_err, eventLen, String("Failed!"));
        
        // 升级失败时也删除文件，避免占用空间
        if (LittleFS.exists(filePath))
        {
            if (LittleFS.remove(filePath))
            {
                LOGD("Firmware file deleted after failure: %s", filePath);
            }
        }
        
        // 立即处理 Web 服务器，确保事件被发送到前端
        webServerHandleClient();
        delay(100);
        webServerHandleClient();
    }
    ledControl.modeLED.mode = LED_OFF;
    vars.zbFlashing = false;
    if (mqttCfg.enable && !vars.mqttConn)
    {
        connectToMqtt();
    }
}