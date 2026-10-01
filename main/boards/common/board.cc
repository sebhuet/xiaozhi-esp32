#include "board.h"
#include "assets/lang_config.h"
#include "display/display.h"
#include "privacy.h"
#include "settings.h"
#include "system_info.h"

#include <esp_chip_info.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_random.h>

#define TAG "Board"

Board::Board() {
    // Privacy hardening: a constant UUID is reported instead of a per-device one (see privacy.h).
    uuid_ = privacy::kUuid;
    ESP_LOGI(TAG, "UUID=%s SKU=%s", uuid_.c_str(), BOARD_NAME);
}

std::string Board::GenerateUuid() {
    // UUID v4 需要 16 字节的随机数据
    uint8_t uuid[16];

    // 使用 ESP32 的硬件随机数生成器
    esp_fill_random(uuid, sizeof(uuid));

    // 设置版本 (版本 4) 和变体位
    uuid[6] = (uuid[6] & 0x0F) | 0x40;  // 版本 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80;  // 变体 1

    // 将字节转换为标准的 UUID 字符串格式
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", uuid[0],
             uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9],
             uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    return std::string(uuid_str);
}

bool Board::GetBatteryLevel(int& level, bool& charging, bool& discharging) { return false; }

bool Board::GetTemperature(float& esp32temp) { return false; }

Display* Board::GetDisplay() {
    static NoDisplay display;
    return &display;
}

Camera* Board::GetCamera() { return nullptr; }

Led* Board::GetLed() {
    static NoLed led;
    return &led;
}

std::string Board::GetSystemInfoJson() {
    /*
        {
            "version": 2,
            "flash_size": 4194304,
            "psram_size": 0,
            "minimum_free_heap_size": 123456,
            "mac_address": "00:00:00:00:00:00",
            "uuid": "00000000-0000-0000-0000-000000000000",
            "chip_model_name": "esp32s3",
            "chip_info": {
                "model": 1,
                "cores": 2,
                "revision": 0,
                "features": 0
            },
            "application": {
                "name": "my-app",
                "version": "1.0.0",
                "compile_time": "2021-01-01T00:00:00Z"
                "idf_version": "4.2-dev"
                "elf_sha256": ""
            },
            "partition_table": [
                "app": {
                    "label": "app",
                    "type": 1,
                    "subtype": 2,
                    "address": 0x10000,
                    "size": 0x100000
                }
            ],
            "ota": {
                "label": "ota_0"
            },
            "board": {
                ...
            }
        }
    */
    std::string json = R"({"version":2,"language":")" + std::string(Lang::CODE) + R"(",)";
    json += R"("flash_size":)" + std::to_string(privacy::kFlashSize) + R"(,)";
    json += R"("minimum_free_heap_size":")" + std::to_string(privacy::kMinimumFreeHeapSize) +
            R"(",)";
    json += R"("mac_address":")" + SystemInfo::GetMacAddress() + R"(",)";
    json += R"("uuid":")" + uuid_ + R"(",)";
    json += R"("chip_model_name":")" + SystemInfo::GetChipModelName() + R"(",)";

    // Constant chip description: the exact silicon revision is not reported.
    json += R"("chip_info":{"model":9,"cores":2,"revision":0,"features":0},)";

    auto app_desc = esp_app_get_description();
    json += R"("application":{)";
    json += R"("name":")" + std::string(app_desc->project_name) + R"(",)";
    json += R"("version":")" + std::string(app_desc->version) + R"(",)";
    json += R"("compile_time":")" + std::string(privacy::kCompileTime) + R"(",)";
    json += R"("idf_version":")" + std::string(app_desc->idf_ver) + R"(",)";
    json += R"("elf_sha256":")" + std::string(privacy::kElfSha256) + R"(")";
    json += R"(},)";

    // The real partition layout is not reported.
    json += R"("partition_table":[],)";
    json += R"("ota":{"label":"ota_0"},)";

    // Append display info
    auto display = GetDisplay();
    if (display) {
        json += R"("display":{)";
        if (display->IsMonochrome()) {
            json += R"("monochrome":)" + std::string("true") + R"(,)";
        } else {
            json += R"("monochrome":)" + std::string("false") + R"(,)";
        }
        json += R"("width":)" + std::to_string(display->width()) + R"(,)";
        json += R"("height":)" + std::to_string(display->height()) + R"(,)";
        json.pop_back();  // Remove the last comma
    }
    json += R"(},)";

    json += R"("board":)" + GetBoardJson();

    // Close the JSON object
    json += R"(})";
    return json;
}
