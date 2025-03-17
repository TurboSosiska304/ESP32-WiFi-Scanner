# ESP32 WiFi Scanner

A powerful WiFi scanner built on the ESP32 platform, featuring a web interface for network analysis, distance calibration, and real-time device monitoring. Designed for both hobbyists and advanced users, this project provides detailed insights into WiFi networks and ESP32 performance.

## Features
- **Network Scanning**: Detects nearby WiFi networks with details including SSID, RSSI (signal strength), channel, and encryption type.
- **Detailed Analysis**: Displays advanced network info such as BSSID, frequency, channel width, and estimated distance based on RSSI calibration.
- **Data Export**: Exports scan results to JSON for external analysis or integration.
- **Device Statistics**: Monitors ESP32 metrics like chip temperature, uptime, free memory, connected clients, and data throughput.
- **Adaptive CPU Frequency**: Dynamically adjusts CPU clock speed (80/160/240 MHz) based on workload (e.g., scanning, client activity, or idle state).
- **Access Point Mode**: Operates as a standalone AP with a configurable SSID and password.

## Requirements
- **Hardware**: ESP32 module (e.g., ESP32-S3 recommended for optimal performance).
- **Software**:
  - Arduino IDE (v1.8.x or later) or PlatformIO.
  - Libraries: `WiFi`, `WebServer`, `esp_wifi` (included with ESP32 core).
- **Development Environment**: Basic familiarity with C++ and ESP32 programming.

## Installation
1. **Clone the Repository**:   https://github.com/TurboSosiska304/ESP32-WiFi-Scanner.git

2. **Open the Project**:
- Launch Arduino IDE and open `src/main.cpp`.
- Alternatively, use PlatformIO: import the project folder and build.
3. **Install Dependencies**:
- In Arduino IDE, go to `Sketch > Include Library > Manage Libraries` and install `WiFi` and `WebServer` if not already present.
- The `esp_wifi` library is part of the ESP32 core and requires no additional installation.
4. **Configure and Upload**:
- Adjust `apSSID` and `apPassword` in `src/main.cpp` if desired (defaults: `ESP32-WiFi-Scanner` / `12345678`).
- Connect your ESP32 via USB, select the correct board and port in Arduino IDE, and upload the code.

## Usage
1. **Connect to the Access Point**:
- Power on the ESP32 and connect to the WiFi network `ESP32-WiFi-Scanner` using the password `12345678`.
- The device operates in AP mode with a default IP address of `192.168.4.1`.
2. **Access the Web Interface**:
- Open a browser and navigate to `http://192.168.4.1`.
- Use the main page to scan networks, "Details" for in-depth analysis, or "Statistics" for device monitoring.
3. **Advanced Features**:
- **Calibration**: On the "Details" page, adjust `P0` (reference RSSI at 1m) and `n` (path loss exponent) for accurate distance estimation.
- **Export**: Download scan results as a JSON file via the "Export JSON" button.
- **Channel Management**: Change the AP channel on the "Statistics" page to optimize performance.
4. **Monitoring**:
- Check real-time stats like CPU frequency, memory usage, and client connections.
- The CPU adapts automatically: 80 MHz (idle), 160 MHz (stats viewing), 240 MHz (scanning/export).

## Technical Details
- **WiFi Scanning**: Utilizes `WiFi.scanNetworks()` with low-level `esp_wifi` for detailed BSSID and channel data.
- **Web Server**: Built with the `WebServer` library, serving HTML, JSON, and handling POST requests.
- **CPU Frequency Control**: Implements `setCpuFrequencyMhz()` with a custom algorithm based on activity timers and flags (`isScanning`, `isExporting`, `isStatsActive`).
- **Power Settings**: Fixed AP transmit power at 17 dBm via `WiFi.setTxPower(WIFI_POWER_17dBm)`.

## Debugging
- Open the Serial Monitor (115200 baud) to view startup logs, IP address, and CPU frequency changes.
- Example output:
- Access Point created
- IP address: 192.168.4.1
- Switching to high frequency: 240 MHz

## Customization
- Modify `IDLE_TIMEOUT` (default: 10000 ms) to adjust how long the device stays at high frequency after activity.
- Extend the project by adding new endpoints (e.g., `/restart`) or integrating with external APIs.
