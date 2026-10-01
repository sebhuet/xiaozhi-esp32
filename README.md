# An MCP-based Chatbot

(English | [中文](README_zh.md) | [日本語](README_ja.md))

> **Fork notice.** This fork (branch `xingmiao`) hardens the firmware's privacy: it reports constant fictitious
> device data and refuses remote firmware updates. See [Security and privacy audit](#security-and-privacy-audit-v250).

## Introduction

👉 [Human: Give AI a camera vs AI: Instantly finds out the owner hasn't washed hair for three days【bilibili】](https://www.bilibili.com/video/BV1bpjgzKEhd/)

👉 [Handcraft your AI girlfriend, beginner's guide【bilibili】](https://www.bilibili.com/video/BV1XnmFYLEJN/)

As a voice interaction entry, the XiaoZhi AI chatbot leverages the AI capabilities of large models like Qwen / DeepSeek, and achieves multi-terminal control via the MCP protocol.

<img src="docs/mcp-based-graph.jpg" alt="Control everything via MCP" width="320">

## Recent Updates

- The project now requires ESP-IDF v6.0.1 or later. [ESP-IDF v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1) is the recommended SDK. ESP-IDF 5.x is no longer supported. The current matrix contains 171 variants; ESP32-S31 builds require IDF 6.1 or later.
- MQTT and BluFi cryptographic code has migrated to PSA Crypto. IDF 6 component splits and third-party dependency compatibility have also been addressed.
- Audio pipeline concurrency, MQTT/UDP packet validation, and release-matrix selection have been hardened.
- ESP32-P4 Rev1 and Rev3 are both supported on IDF 6 with ESP-SR 2.4.7.

### Features Implemented

- Wi-Fi, wired Ethernet, USB RNDIS, and ML307/EC801E or NT26 Cat.1 4G networking; supported boards can switch between Wi-Fi and 4G
- Offline voice wake-up with [ESP-SR](https://github.com/espressif/esp-sr), including customizable wake words
- Two communication transports: [WebSocket](docs/websocket.md) and [MQTT + UDP](docs/mqtt-udp.md)
- Opus audio streaming with conventional streaming ASR + LLM + TTS pipelines and Realtime end-to-end voice models; AEC-capable hardware supports realtime full-duplex interaction
- Speaker recognition, identifies the current speaker [3D Speaker](https://github.com/modelscope/3D-Speaker)
- OLED / LCD displays with emoji and rich expression support, plus camera vision input on supported boards
- Battery display and power management
- 39 interface languages, with localized voice prompts where available and English fallback
- ESP32, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-S3, and ESP32-P4 chip platforms
- Wi-Fi provisioning through hotspot or BluFi
- Device-side MCP for device control (Speaker, LED, Servo, GPIO, etc.)
- Cloud-side MCP to extend large model capabilities (smart home control, PC desktop operation, knowledge search, email, etc.)
- Customizable wake words, fonts, emojis, and chat backgrounds with online web-based editing ([Custom Assets Generator](https://github.com/78/xiaozhi-assets-generator))

## Security and privacy audit (v2.5.0)

Scope: reading of `main/ota.cc`, `main/application.cc`, `main/mcp_server.cc`, `main/protocols/`,
`main/boards/common/board.cc` and `main/boards/common/wifi_board.cc`, plus the strings of a compiled
`esp-vocat` build. No network capture was made, so this describes what the code does, not what a given
server operator does with it.

### What the device sends besides conversations

As soon as Wi-Fi is up, the firmware calls the OTA URL (default `https://api.tenclass.net/xiaozhi/ota/`,
changed with `CONFIG_OTA_URL` at build time or the NVS key `wifi:ota_url` at run time) with an HTTPS `POST`:

| Where | Content |
|---|---|
| Header `Device-Id` | MAC address |
| Header `Client-Id` | UUID generated once and stored in NVS |
| Headers `User-Agent`, `Accept-Language`, `Activation-Version` | board name and version, UI language |
| Header `Serial-Number` | only if the eFuse `USER_DATA` block holds a serial number |
| Body, hardware | flash size, minimum free heap, chip model, revision, core count |
| Body, application | name, version, compile time, ESP-IDF version, **SHA-256 of the binary** |
| Body, layout | the complete partition table and the running OTA partition |
| Body, display and board | display size, board type, name and manufacturer, MAC address |
| Body, network | **Wi-Fi SSID, signal strength, channel and local IP address** (sent once connected) |

The activation request carries an HMAC and the serial number if the eFuses hold them, and `{}` otherwise.
The conversation connection sends the `Authorization` token, `Protocol-Version`, `Device-Id` and `Client-Id`,
and the audio of the wake word at the start of a conversation (`CONFIG_SEND_WAKE_WORD_DATA`, enabled by
default). The device clock comes from the `server_time` field of the OTA response; no NTP server is hard-coded.
No other telemetry was found in the application code.

### What the server can do to a device

- **Install firmware, automatically.** The `firmware` object of the OTA response is acted on at boot:
  `Application::CheckNewVersion` calls `UpgradeFirmware` whenever a newer version is announced, and
  `"force": 1` installs any version. Nothing is signed or verified beyond the ESP-IDF image checks, and secure
  boot is off by default.
- **Install firmware through MCP.** The user-only tool `self.upgrade_firmware` does the same from a URL.
- **Choose where the audio goes.** The `mqtt` and `websocket` sections of the OTA response are written to NVS
  and select the conversation endpoint and its credentials.
- **Replace assets.** The user-only tool `self.assets.set_download_url` makes the next boot download a new
  assets partition (fonts, animations, the wake word model).
- **Make the device fetch any URL.** A `notify` message carries an `audio_url` that the device downloads and plays.
- **Make the device send its screen.** On LVGL boards, the user-only tool `self.screen.preview_image` downloads and
  shows an image from a URL and, when `CONFIG_LV_USE_SNAPSHOT` is set, `self.screen.snapshot` uploads a screenshot of
  the display to a URL. The `esp-vocat` build registers neither.
- **Read the device state.** `self.get_device_status` returns volume, brightness, theme, battery, the Wi-Fi SSID
  and signal strength, and the chip temperature; the user-only `self.get_system_info` returns the full body above.

### Why it matters

The OTA endpoint is a trust anchor. Whoever operates it receives the data above, can replace the firmware of
every device that contacts it, and can point the audio of the device at another server. Running your own
server is the only way to keep all of this in your hands.

### Mitigations in this fork (branch `xingmiao`)

- `main/privacy.h` holds constant, fictitious values that replace the MAC address, the UUID, the Wi-Fi SSID, signal,
  channel and IP address, the SHA-256 of the binary, the compile time, the partition table and the chip revision in
  every request, header and MCP result. They are identical for every device.
- Firmware updates chosen by a server are refused (`kAllowRemoteFirmwareUpdate = false`), both in
  `Application::UpgradeFirmware`, the single path used by the OTA response and by MCP, and in `Ota::StartUpgrade`.
  Flash over USB instead.
- The MCP tools `self.upgrade_firmware` and `self.assets.set_download_url` are no longer registered.
- No OTA request is made by default (`kDefaultOtaUrl` is empty, `CONFIG_OTA_URL` is no longer used): nothing is sent and
  nothing is received. To enable it, store the URL of your own server in the NVS key `wifi:ota_url`.
- The conversation endpoint is never taken from a server: the `mqtt` and `websocket` sections of an OTA response are
  ignored, MQTT is no longer selected, and the WebSocket endpoint is the one stored locally in the NVS namespace
  `websocket` (keys `url`, `token`, `version`).
- `CONFIG_SEND_WAKE_WORD_DATA` now defaults to `n`: the audio of the wake word is not sent.
- Still real because functional: volume, brightness, theme, battery level, chip temperature, application name and version.

### What the mitigations do not cover

- Whatever you say in a conversation reaches the conversation server you configured, and the connection itself
  discloses the public IP address of your network to it.
- `notify` messages sent by that server can still make the device fetch a URL.
- Network metadata (IP address, DNS, TLS fingerprint, timing) cannot be hidden by the firmware, and identical constants
  are themselves a recognisable signature.
- Without an OTA response the device has no clock source: a `wss://` endpoint needs a valid time to verify its
  certificate, so use `ws://` on a trusted network or provide the time another way.
- Secure boot and flash encryption are not enabled.
- Only the Wi-Fi board classes were patched: the 4G (ML307) and RNDIS boards still report their own identifiers.

## Hardware

### Breadboard DIY Practice

See the Feishu document tutorial:

👉 ["XiaoZhi AI Chatbot Encyclopedia"](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb?from=from_copylink)

Breadboard demo:

![Breadboard Demo](docs/v1/wiring2.jpg)

### Supports 138 Board Directories and 171 Release Variants (Partial List)

- <a href="https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban" target="_blank" title="LiChuang ESP32-S3 Development Board">LiChuang ESP32-S3 Development Board</a>
- <a href="https://github.com/espressif/esp-box" target="_blank" title="Espressif ESP32-S3-BOX-3">Espressif ESP32-S3-BOX-3</a>
- <a href="https://docs.m5stack.com/zh_CN/core/CoreS3" target="_blank" title="M5Stack CoreS3">M5Stack CoreS3</a>
- <a href="https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base" target="_blank" title="AtomS3R + Echo Base">M5Stack AtomS3R + Echo Base</a>
- <a href="https://gf.bilibili.com/item/detail/1108782064" target="_blank" title="Magic Button 2.4">Magic Button 2.4</a>
- <a href="https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm" target="_blank" title="Waveshare ESP32-S3-Touch-AMOLED-1.8">Waveshare ESP32-S3-Touch-AMOLED-1.8</a>
- <a href="https://github.com/Xinyuan-LilyGO/T-Circle-S3" target="_blank" title="LILYGO T-Circle-S3">LILYGO T-Circle-S3</a>
- <a href="https://oshwhub.com/tenclass01/xmini_c3" target="_blank" title="XiaGe Mini C3">XiaGe Mini C3</a>
- <a href="https://oshwhub.com/movecall/cuican-ai-pendant-lights-up-y" target="_blank" title="Movecall CuiCan ESP32S3">CuiCan AI Pendant</a>
- <a href="https://github.com/WMnologo/xingzhi-ai" target="_blank" title="WMnologo-Xingzhi-1.54">WMnologo-Xingzhi-1.54TFT</a>
- <a href="https://www.seeedstudio.com/SenseCAP-Watcher-W1-A-p-5979.html" target="_blank" title="SenseCAP Watcher">SenseCAP Watcher</a>
- <a href="https://www.bilibili.com/video/BV1BHJtz6E2S/" target="_blank" title="ESP-HI Low Cost Robot Dog">ESP-HI Low Cost Robot Dog</a>

<div style="display: flex; justify-content: space-between;">
  <a href="docs/v1/lichuang-s3.jpg" target="_blank" title="LiChuang ESP32-S3 Development Board">
    <img src="docs/v1/lichuang-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/espbox3.jpg" target="_blank" title="Espressif ESP32-S3-BOX3">
    <img src="docs/v1/espbox3.jpg" width="240" />
  </a>
  <a href="docs/v1/m5cores3.jpg" target="_blank" title="M5Stack CoreS3">
    <img src="docs/v1/m5cores3.jpg" width="240" />
  </a>
  <a href="docs/v1/atoms3r.jpg" target="_blank" title="AtomS3R + Echo Base">
    <img src="docs/v1/atoms3r.jpg" width="240" />
  </a>
  <a href="docs/v1/magiclick.jpg" target="_blank" title="Magic Button 2.4">
    <img src="docs/v1/magiclick.jpg" width="240" />
  </a>
  <a href="docs/v1/waveshare.jpg" target="_blank" title="Waveshare ESP32-S3-Touch-AMOLED-1.8">
    <img src="docs/v1/waveshare.jpg" width="240" />
  </a>
  <a href="docs/v1/lilygo-t-circle-s3.jpg" target="_blank" title="LILYGO T-Circle-S3">
    <img src="docs/v1/lilygo-t-circle-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/xmini-c3.jpg" target="_blank" title="XiaGe Mini C3">
    <img src="docs/v1/xmini-c3.jpg" width="240" />
  </a>
  <a href="docs/v1/movecall-cuican-esp32s3.jpg" target="_blank" title="CuiCan">
    <img src="docs/v1/movecall-cuican-esp32s3.jpg" width="240" />
  </a>
  <a href="docs/v1/wmnologo_xingzhi_1.54.jpg" target="_blank" title="WMnologo-Xingzhi-1.54">
    <img src="docs/v1/wmnologo_xingzhi_1.54.jpg" width="240" />
  </a>
  <a href="docs/v1/sensecap_watcher.jpg" target="_blank" title="SenseCAP Watcher">
    <img src="docs/v1/sensecap_watcher.jpg" width="240" />
  </a>
  <a href="docs/v1/esp-hi.jpg" target="_blank" title="ESP-HI Low Cost Robot Dog">
    <img src="docs/v1/esp-hi.jpg" width="240" />
  </a>
</div>

## Software

### Firmware Flashing

For beginners, it is recommended to use the firmware that can be flashed without setting up a development environment.

The firmware connects to the official [xiaozhi.me](https://xiaozhi.me) server by default. Personal users can register an account to use the Qwen real-time model for free.

👉 [Beginner's Firmware Flashing Guide](https://ccnphfhqs21z.feishu.cn/wiki/Zpz4wXBtdimBrLk25WdcXzxcnNS)

### Development Environment

- Cursor or VSCode
- Install the ESP-IDF plugin. The minimum SDK is [ESP-IDF v6.0.1](https://github.com/espressif/esp-idf/releases/tag/v6.0.1); [ESP-IDF v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1) is recommended. ESP-IDF 5.x is not supported.
- Linux is better than Windows for faster compilation and fewer driver issues
- This project uses Google C++ code style, please ensure compliance when submitting code

### Developer Documentation

- [Custom Board Guide](docs/custom-board.md) - Learn how to create custom boards for XiaoZhi AI
- [MCP Protocol IoT Control Usage](docs/mcp-usage.md) - Learn how to control IoT devices via MCP protocol
- [MCP Protocol Interaction Flow](docs/mcp-protocol.md) - Device-side MCP protocol implementation
- [MQTT + UDP Hybrid Communication Protocol Document](docs/mqtt-udp.md)
- [A detailed WebSocket communication protocol document](docs/websocket.md)

## Large Model Configuration

If you already have a XiaoZhi AI chatbot device and have connected to the official server, you can log in to the [xiaozhi.me](https://xiaozhi.me) console for configuration.

👉 [Backend Operation Video Tutorial (Old Interface)](https://www.bilibili.com/video/BV1jUCUY2EKM/)

## Related Open Source Projects

For server deployment on personal computers, refer to the following open-source projects:

- [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) Python server
- [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) Java server
- [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) Golang server
- [hackers365/xiaozhi-esp32-server-golang](https://github.com/hackers365/xiaozhi-esp32-server-golang) Golang server

Other client projects using the XiaoZhi communication protocol:

- [huangjunsen0406/py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi) Python client
- [TOM88812/xiaozhi-android-client](https://github.com/TOM88812/xiaozhi-android-client) Android client
- [100askTeam/xiaozhi-linux](http://github.com/100askTeam/xiaozhi-linux) Linux client by 100ask
- [78/xiaozhi-sf32](https://github.com/78/xiaozhi-sf32) Bluetooth chip firmware by Sichuan
- [QuecPython/solution-xiaozhiAI](https://github.com/QuecPython/solution-xiaozhiAI) QuecPython firmware by Quectel

Custom Assets Tools:

- [78/xiaozhi-assets-generator](https://github.com/78/xiaozhi-assets-generator) Custom Assets Generator (Wake words, fonts, emojis, backgrounds)

## About the Project

This is an open-source ESP32 project, released under the MIT license, allowing anyone to use it for free, including for commercial purposes.

We hope this project helps everyone understand AI hardware development and apply rapidly evolving large language models to real hardware devices.

If you have any ideas or suggestions, please feel free to raise Issues or join our [Discord](https://discord.gg/C759fGMBcZ) or QQ group: 1095994019

## Star History

<a href="https://star-history.com/#78/xiaozhi-esp32&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
 </picture>
</a>
