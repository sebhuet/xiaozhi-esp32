#pragma once

// Privacy hardening of the xingmiao fork.
//
// Everything a server could use to recognise this device, or the network it sits on, is replaced by the
// constant values below before it leaves the device: identifiers, build fingerprint, partition layout and
// Wi-Fi details. The values are fictitious on purpose and identical for every device running this build.
//
// Remote firmware updates are refused: neither the OTA response (even with "force") nor the MCP tools may
// install firmware. Flash through USB instead.
//
// What stays real, because it is functional: volume, brightness, theme, battery level and chip temperature
// reported by `self.get_device_status`, and the application name, version and ESP-IDF version.

namespace privacy {

// Locally administered address (second digit 2): it cannot be mistaken for a real vendor MAC.
inline constexpr const char* kMacAddress = "02:00:00:00:00:01";
inline constexpr const char* kUuid = "00000000-0000-4000-8000-000000000001";

// Network details reported to servers.
inline constexpr const char* kWifiSsid = "wifi";
inline constexpr int kWifiRssi = -55;
inline constexpr const char* kWifiSignal = "medium";
inline constexpr int kWifiChannel = 6;
inline constexpr const char* kIpAddress = "192.168.1.10";

// Build and hardware fingerprint reported to servers.
inline constexpr int kFlashSize = 16 * 1024 * 1024;
inline constexpr int kMinimumFreeHeapSize = 100000;
inline constexpr const char* kCompileTime = "2025-01-01T00:00:00Z";
inline constexpr const char* kElfSha256 =
    "0000000000000000000000000000000000000000000000000000000000000000";

// Set to true only to restore the upstream behaviour of installing firmware chosen by a server.
inline constexpr bool kAllowRemoteFirmwareUpdate = false;

}  // namespace privacy
