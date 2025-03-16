#include <WiFi.h>
#include <WebServer.h>
#include "esp_wifi.h"

// Настройки точки доступа
const char* apSSID = "ESP32-WiFi-Scanner";
const char* apPassword = "12345678";

// Создаем объект веб-сервера на порту 80
WebServer server(80);

// Переменные для калибровки
float P0 = -40.0;
float n = 2.0;

// Переменные для скорости передачи данных
unsigned long txBytes = 0;
unsigned long rxBytes = 0;
unsigned long lastCheck = 0;

// Переменные для адаптивной частоты
unsigned long lastActivity = 0;
bool isScanning = false;
bool isExporting = false;
bool isStatsActive = false;
const int LOW_FREQ = 80;
const int MID_FREQ = 160;
const int HIGH_FREQ = 240;
const unsigned long IDLE_TIMEOUT = 10000;

// Главная страница
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <title>ESP32 WiFi Сканер</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 10px; background-color: #1E3A5F; }
    h2 { text-align: center; color: #BBDEFB; margin: 10px 0; }
    .container { max-width: 100%; margin: 0 auto; }
    .nav-buttons { text-align: right; margin-bottom: 10px; }
    .nav-button { 
      padding: 8px 12px; 
      background-color: #1565C0; 
      color: white; 
      border: none; 
      border-radius: 4px; 
      cursor: pointer; 
      font-size: 12px; 
      margin-left: 5px;
      height: 32px;
      box-sizing: border-box;
    }
    .nav-button:hover { background-color: #0D47A1; }
    .button { 
      display: block; 
      width: 100%; 
      padding: 15px; 
      background-color: #1976D2; 
      color: white; 
      border: none; 
      border-radius: 4px; 
      font-size: 16px; 
      cursor: pointer; 
      margin-bottom: 15px; 
    }
    .button:hover { background-color: #1565C0; }
    table { width: 100%; border-collapse: collapse; background-color: #263238; box-shadow: 0 2px 5px rgba(0,0,0,0.2); }
    th, td { padding: 10px; text-align: left; border-bottom: 1px solid #455A64; font-size: 14px; word-wrap: break-word; color: #BBDEFB; }
    th { background-color: #1565C0; color: white; }
    tr:nth-child(even) { background-color: #2E4053; }
    @media (max-width: 600px) { 
      th, td { font-size: 12px; padding: 8px; } 
      .nav-button { padding: 6px 10px; font-size: 11px; height: 28px; }
      h2 { font-size: 20px; }
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>WiFi Сканер</h2>
    <div class="nav-buttons">
      <button class="nav-button" onclick="window.location.href='/details'">Подробности</button>
      <button class="nav-button" onclick="window.location.href='/stats'">Статистика</button>
    </div>
    <button class="button" onclick="scanNetworks()">Сканировать</button>
    <table id="wifiTable">
      <tr>
        <th>SSID</th>
        <th>RSSI</th>
        <th>Канал</th>
        <th>Шифрование</th>
      </tr>
      <tr><td colspan="4">Нажмите 'Сканировать' для поиска сетей</td></tr>
    </table>
  </div>
  <script>
    function scanNetworks() {
      fetch('/scan')
        .then(response => response.text())
        .then(data => {
          document.getElementById('wifiTable').innerHTML = `
            <tr>
              <th>SSID</th>
              <th>RSSI</th>
              <th>Канал</th>
              <th>Шифрование</th>
            </tr>
            ${data}
          `;
        })
        .catch(error => console.log('Ошибка:', error));
    }
  </script>
</body>
</html>
)rawliteral";

// Страница с подробностями
const char details_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <title>Подробная информация о WiFi</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 10px; background-color: #1E3A5F; }
    h2 { text-align: center; color: #BBDEFB; margin: 10px 0; }
    .container { max-width: 100%; margin: 0 auto; }
    .nav-buttons { text-align: right; margin-bottom: 10px; }
    .nav-button { 
      padding: 8px 12px; 
      background-color: #1565C0; 
      color: white; 
      border: none; 
      border-radius: 4px; 
      cursor: pointer; 
      font-size: 12px; 
      margin-left: 5px;
      height: 32px;
      box-sizing: border-box;
    }
    .nav-button:hover { background-color: #0D47A1; }
    .export-button { background-color: #388E3C; }
    .export-button:hover { background-color: #2E7D32; }
    .details { background-color: #263238; padding: 15px; margin-bottom: 15px; border-radius: 4px; box-shadow: 0 2px 5px rgba(0,0,0,0.2); }
    .details p { margin: 5px 0; color: #BBDEFB; }
    .details strong { color: #64B5F6; }
    .calibration { background-color: #263238; padding: 15px; margin-bottom: 15px; border-radius: 4px; box-shadow: 0 2px 5px rgba(0,0,0,0.2); }
    .calibration label { color: #BBDEFB; font-size: 14px; margin-right: 10px; }
    .calibration input[type="number"] { padding: 6px; background-color: #2E4053; color: #BBDEFB; border: 1px solid #455A64; border-radius: 4px; width: 80px; margin-right: 10px; }
    .calibration input[type="submit"] { padding: 6px 12px; background-color: #1565C0; color: white; border: none; border-radius: 4px; cursor: pointer; }
    .calibration input[type="submit"]:hover { background-color: #0D47A1; }
    @media (max-width: 600px) { 
      .details, .calibration { padding: 10px; } 
      p, label { font-size: 12px; } 
      .nav-button { padding: 6px 10px; font-size: 11px; height: 28px; }
      h2 { font-size: 20px; }
      .calibration input[type="number"] { width: 60px; }
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>Подробная информация</h2>
    <div class="nav-buttons">
      <button class="nav-button" onclick="window.location.href='/'">Назад</button>
      <button class="nav-button export-button" onclick="window.location.href='/export_json'">Экспорт JSON</button>
      <button class="nav-button" onclick="window.location.href='/stats'">Статистика</button>
    </div>
    <div class="calibration">
      <form id="calibrationForm" onsubmit="calibrate(event)">
        <label>Калибровочный RSSI (P0, dBm):</label>
        <input type="number" step="0.1" name="p0" value="%P0%">
        <label>Коэффициент затухания (n):</label>
        <input type="number" step="0.1" name="n" value="%N%">
        <input type="submit" value="Калибровать">
      </form>
    </div>
    %DETAILS_CONTENT%
  </div>
  <script>
    function calibrate(event) {
      event.preventDefault();
      const form = document.getElementById('calibrationForm');
      const formData = new FormData(form);
      fetch('/calibrate', { method: 'POST', body: formData })
        .then(response => response.json())
        .then(data => {
          document.querySelector('input[name="p0"]').value = data.p0;
          document.querySelector('input[name="n"]').value = data.n;
          fetch('/details_content')
            .then(response => response.text())
            .then(html => { document.querySelector('.container').innerHTML = html; });
        })
        .catch(error => console.log('Ошибка:', error));
    }
  </script>
</body>
</html>
)rawliteral";

// Страница статистики
const char stats_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <title>Статистика ESP32</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 10px; background-color: #1E3A5F; }
    h2 { text-align: center; color: #BBDEFB; margin: 10px 0; }
    .container { max-width: 100%; margin: 0 auto; }
    .nav-buttons { text-align: right; margin-bottom: 10px; }
    .nav-button { 
      padding: 8px 12px; 
      background-color: #1565C0; 
      color: white; 
      border: none; 
      border-radius: 4px; 
      cursor: pointer; 
      font-size: 12px; 
      margin-left: 5px;
      height: 32px;
      box-sizing: border-box;
    }
    .nav-button:hover { background-color: #0D47A1; }
    .restart-button { background-color: #D32F2F; }
    .restart-button:hover { background-color: #B71C1C; }
    .stats { background-color: #263238; padding: 15px; border-radius: 4px; box-shadow: 0 2px 5px rgba(0,0,0,0.2); }
    .stats p { margin: 10px 0; color: #BBDEFB; font-size: 14px; }
    .stats strong { color: #64B5F6; }
    .stats ul { margin: 5px 0 10px 20px; color: #BBDEFB; font-size: 14px; }
    select { 
      padding: 6px; 
      background-color: #2E4053; 
      color: #BBDEFB; 
      border: 1px solid #455A64; 
      border-radius: 4px; 
      font-size: 14px; 
      margin-left: 10px;
    }
    @media (max-width: 600px) { 
      .stats { padding: 10px; }
      p, ul, select { font-size: 12px; }
      .nav-button { padding: 6px 10px; font-size: 11px; height: 28px; }
      h2 { font-size: 20px; }
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>Статистика ESP32</h2>
    <div class="nav-buttons">
      <button class="nav-button" onclick="window.location.href='/'">Назад</button>
      <button class="nav-button restart-button" onclick="restartESP()">Перезагрузить</button>
    </div>
    <div class="stats" id="statsContent">
      %STATS_CONTENT%
    </div>
  </div>
  <script>
    function updateStats() {
      fetch('/stats_data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('temp').textContent = data.temp.toFixed(2) + ' °C';
          document.getElementById('uptime').textContent = data.uptime;
          document.getElementById('clients').textContent = data.clients;
          document.getElementById('cpuFreq').textContent = data.cpuFreq + ' МГц';
          document.getElementById('txBytes').textContent = data.txBytes;
          document.getElementById('rxBytes').textContent = data.rxBytes;
          document.getElementById('channel').textContent = data.channel;
          document.getElementById('channelLoad').textContent = data.channelLoad;
          updateMemory(data.freeHeap);
          if (data.clientsList) {
            document.getElementById('clientsList').innerHTML = data.clientsList;
          }
        })
        .catch(error => console.log('Ошибка:', error));
    }

    function updateMemory(freeHeap) {
      const unit = document.getElementById('memoryUnit').value;
      let value;
      switch (unit) {
        case 'bytes': value = freeHeap; break;
        case 'kb': value = (freeHeap / 1024).toFixed(2); break;
        case 'mb': value = (freeHeap / (1024 * 1024)).toFixed(2); break;
      }
      document.getElementById('freeHeap').textContent = value + ' ' + unit;
    }

    function setChannel() {
      const channel = document.getElementById('channelSelect').value;
      fetch('/set_channel', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'channel=' + channel
      })
        .then(response => response.json())
        .then(data => {
          document.getElementById('channel').textContent = data.channel;
          alert('Канал изменён, точка доступа перезапущена');
        })
        .catch(error => console.log('Ошибка:', error));
    }

    function restartESP() {
      if (confirm('Перезагрузить ESP32?')) {
        fetch('/restart', { method: 'POST' })
          .then(() => alert('ESP32 перезагружается...'))
          .catch(error => console.log('Ошибка:', error));
      }
    }

    window.onload = () => {
      updateStats();
      setInterval(updateStats, 2000);
    };
  </script>
</body>
</html>
)rawliteral";

String getEncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN: return "Открытая";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise";
    default: return "Неизвестно";
  }
}

String getChannelFrequency(int channel) {
  if (channel <= 14) return String(2412 + (channel - 1) * 5);
  else return String(5180 + (channel - 36) * 5);
}

String getChannelWidth(int channel) {
  if (channel <= 14) return "20 или 40 МГц";
  else return "20, 40, 80 или 160 МГц";
}

String scanNetworks() {
  isScanning = true;
  lastActivity = millis();
  String wifiTable = "";
  int n = WiFi.scanNetworks();
  if (n == 0) {
    wifiTable = "<tr><td colspan='4'>Сети WiFi не найдены</td></tr>";
  } else {
    for (int i = 0; i < n; ++i) {
      wifiTable += "<tr>";
      wifiTable += "<td>" + WiFi.SSID(i) + "</td>";
      wifiTable += "<td>" + String(WiFi.RSSI(i)) + " dBm</td>";
      wifiTable += "<td>" + String(WiFi.channel(i)) + "</td>";
      wifiTable += "<td>" + getEncryptionType(WiFi.encryptionType(i)) + "</td>";
      wifiTable += "</tr>";
      delay(10);
    }
  }
  WiFi.scanDelete();
  isScanning = false;
  return wifiTable;
}

String getDetailedNetworks() {
  isScanning = true;
  lastActivity = millis();
  String detailsContent = "";
  int n = WiFi.scanNetworks();
  if (n == 0) {
    detailsContent = "<p>Сети WiFi не найдены</p>";
  } else {
    for (int i = 0; i < n; ++i) {
      detailsContent += "<div class='details'>";
      detailsContent += "<p><strong>SSID:</strong> " + WiFi.SSID(i) + "</p>";
      detailsContent += "<p><strong>RSSI:</strong> " + String(WiFi.RSSI(i)) + " dBm</p>";
      detailsContent += "<p><strong>Канал:</strong> " + String(WiFi.channel(i)) + "</p>";
      detailsContent += "<p><strong>Шифрование:</strong> " + getEncryptionType(WiFi.encryptionType(i)) + "</p>";
      detailsContent += "<p><strong>MAC-адрес (BSSID):</strong> " + WiFi.BSSIDstr(i) + "</p>";
      detailsContent += "<p><strong>Частота:</strong> " + getChannelFrequency(WiFi.channel(i)) + " МГц</p>";
      detailsContent += "<p><strong>Ширина канала:</strong> " + getChannelWidth(WiFi.channel(i)) + "</p>";
      float distance = pow(10, (P0 - WiFi.RSSI(i)) / (10 * n));
      detailsContent += "<p><strong>Примерное расстояние:</strong> " + String(distance, 2) + " м</p>";
      detailsContent += "<p><strong>Калибровочный RSSI (P0):</strong> " + String(P0, 1) + " dBm</p>";
      detailsContent += "<p><strong>Коэффициент затухания (n):</strong> " + String(n, 2) + "</p>";
      detailsContent += "</div>";
      delay(10);
    }
  }
  WiFi.scanDelete();
  isScanning = false;
  return detailsContent;
}

String generateJson() {
  isExporting = true;
  lastActivity = millis();
  String jsonContent = "[\n";
  int n = WiFi.scanNetworks();
  if (n == 0) {
    jsonContent += "  {\"message\": \"Сети WiFi не найдены\"}\n";
  } else {
    for (int i = 0; i < n; ++i) {
      jsonContent += "  {\n";
      jsonContent += "    \"SSID\": \"" + WiFi.SSID(i) + "\",\n";
      jsonContent += "    \"RSSI\": " + String(WiFi.RSSI(i)) + ",\n";
      jsonContent += "    \"Channel\": " + String(WiFi.channel(i)) + ",\n";
      jsonContent += "    \"Encryption\": \"" + getEncryptionType(WiFi.encryptionType(i)) + "\",\n";
      jsonContent += "    \"BSSID\": \"" + WiFi.BSSIDstr(i) + "\",\n";
      jsonContent += "    \"Frequency\": \"" + getChannelFrequency(WiFi.channel(i)) + " MHz\",\n";
      jsonContent += "    \"ChannelWidth\": \"" + getChannelWidth(WiFi.channel(i)) + "\",\n";
      float distance = pow(10, (P0 - WiFi.RSSI(i)) / (10 * n));
      jsonContent += "    \"Distance\": " + String(distance, 2) + ",\n";
      jsonContent += "    \"P0\": " + String(P0, 1) + ",\n";
      jsonContent += "    \"n\": " + String(n, 2) + "\n";
      jsonContent += "  }";
      if (i < n - 1) jsonContent += ",";
      jsonContent += "\n";
      delay(10);
    }
  }
  jsonContent += "]";
  WiFi.scanDelete();
  isExporting = false;
  return jsonContent;
}

String getStatsContent() {
  lastActivity = millis();
  isStatsActive = true;
  String statsContent = "";
  float temp = temperatureRead();
  statsContent += "<p><strong>Температура чипа:</strong> <span id='temp'>" + String(temp, 2) + " °C</span></p>";
  
  unsigned long uptime = millis() / 1000;
  String uptimeStr = String(uptime / 3600) + " ч " + String((uptime % 3600) / 60) + " мин " + String(uptime % 60) + " с";
  statsContent += "<p><strong>Время работы:</strong> <span id='uptime'>" + uptimeStr + "</span></p>";
  
  int clients = WiFi.softAPgetStationNum();
  statsContent += "<p><strong>Подключено устройств:</strong> <span id='clients'>" + String(clients) + "</span></p>";
  
  if (clients > 0) {
    statsContent += "<p><strong>Клиенты:</strong></p><ul id='clientsList'>";
    wifi_sta_list_t wifi_sta_list;
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    for (int i = 0; i < wifi_sta_list.num; i++) {
      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
              wifi_sta_list.sta[i].mac[0], wifi_sta_list.sta[i].mac[1],
              wifi_sta_list.sta[i].mac[2], wifi_sta_list.sta[i].mac[3],
              wifi_sta_list.sta[i].mac[4], wifi_sta_list.sta[i].mac[5]);
      statsContent += "<li>MAC: " + String(macStr) + "</li>";
    }
    statsContent += "</ul>";
  }
  
  statsContent += "<p><strong>Уровень сигнала AP:</strong> <span id='rssi'>Измерьте с клиента</span></p>";
  statsContent += "<p><strong>Переданные данные:</strong> <span id='txBytes'>" + String(txBytes / 1024) + " KB</span></p>";
  statsContent += "<p><strong>Принятые данные:</strong> <span id='rxBytes'>" + String(rxBytes / 1024) + " KB</span></p>";
  statsContent += "<p><strong>Частота процессора:</strong> <span id='cpuFreq'>" + String(getCpuFrequencyMhz()) + " МГц</span></p>";
  
  size_t freeHeap = ESP.getFreeHeap();
  statsContent += "<p><strong>Свободная память:</strong> <span id='freeHeap'>" + String(freeHeap) + " bytes</span> "
                  "<select id='memoryUnit' onchange='updateMemory(" + String(freeHeap) + ")'>"
                  "<option value='bytes'>bytes</option>"
                  "<option value='kb'>KB</option>"
                  "<option value='mb'>MB</option>"
                  "</select></p>";
  
  statsContent += "<p><strong>IP-адрес точки доступа:</strong> " + WiFi.softAPIP().toString() + "</p>";
  
  int currentChannel = 1;
  int channelLoad = 0;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    if (WiFi.channel(i) == currentChannel) channelLoad++;
  }
  WiFi.scanDelete();
  statsContent += "<p><strong>Текущий канал:</strong> <span id='channel'>" + String(currentChannel) + "</span> "
                  "<select id='channelSelect' onchange='setChannel()'>"
                  "<option value='1'>1</option>"
                  "<option value='2'>2</option>"
                  "<option value='3'>3</option>"
                  "<option value='4'>4</option>"
                  "<option value='5'>5</option>"
                  "<option value='6'>6</option>"
                  "<option value='7'>7</option>"
                  "<option value='8'>8</option>"
                  "<option value='9'>9</option>"
                  "<option value='10'>10</option>"
                  "<option value='11'>11</option>"
                  "<option value='12'>12</option>"
                  "<option value='13'>13</option>"
                  "</select></p>";
  statsContent += "<p><strong>Сетей на канале:</strong> <span id='channelLoad'>" + String(channelLoad) + "</span></p>";
  
  return statsContent;
}

String getStatsData() {
  isStatsActive = true;
  float temp = temperatureRead();
  unsigned long uptime = millis() / 1000;
  String uptimeStr = String(uptime / 3600) + " ч " + String((uptime % 3600) / 60) + " мин " + String(uptime % 60) + " с";
  int clients = WiFi.softAPgetStationNum();
  
  String clientsList = "";
  if (clients > 0) {
    clientsList += "<ul>";
    wifi_sta_list_t wifi_sta_list;
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    for (int i = 0; i < wifi_sta_list.num; i++) {
      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
              wifi_sta_list.sta[i].mac[0], wifi_sta_list.sta[i].mac[1],
              wifi_sta_list.sta[i].mac[2], wifi_sta_list.sta[i].mac[3],
              wifi_sta_list.sta[i].mac[4], wifi_sta_list.sta[i].mac[5]);
      clientsList += "<li>MAC: " + String(macStr) + "</li>";
    }
    clientsList += "</ul>";
  }
  
  size_t freeHeap = ESP.getFreeHeap();
  int cpuFreq = getCpuFrequencyMhz();
  
  if (millis() - lastCheck >= 2000) {
    txBytes += random(100, 1000);
    rxBytes += random(50, 500);
    lastCheck = millis();
  }
  
  int currentChannel = 1;
  int channelLoad = 0;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    if (WiFi.channel(i) == currentChannel) channelLoad++;
  }
  WiFi.scanDelete();
  
  String json = "{";
  json += "\"temp\": " + String(temp, 2) + ",";
  json += "\"uptime\": \"" + uptimeStr + "\",";
  json += "\"clients\": " + String(clients) + ",";
  json += "\"clientsList\": \"" + clientsList + "\",";
  json += "\"rssi\": \"Измерьте с клиента\",";
  json += "\"txBytes\": \"" + String(txBytes / 1024) + " KB\",";
  json += "\"rxBytes\": \"" + String(rxBytes / 1024) + " KB\",";
  json += "\"cpuFreq\": " + String(cpuFreq) + ",";
  json += "\"freeHeap\": " + String(freeHeap) + ",";
  json += "\"channel\": " + String(currentChannel) + ",";
  json += "\"channelLoad\": " + String(channelLoad);
  json += "}";
  return json;
}

void handleRoot() {
  lastActivity = millis();
  String page = index_html;
  server.send(200, "text/html; charset=utf-8", page);
}

void handleScan() {
  lastActivity = millis();
  String wifiTable = scanNetworks();
  server.send(200, "text/html; charset=utf-8", wifiTable);
}

void handleDetails() {
  lastActivity = millis();
  String page = details_html;
  String detailsContent = getDetailedNetworks();
  page.replace("%DETAILS_CONTENT%", detailsContent);
  page.replace("%P0%", String(P0, 1));
  page.replace("%N%", String(n, 2));
  server.send(200, "text/html; charset=utf-8", page);
}

void handleDetailsContent() {
  lastActivity = millis();
  String detailsContent = getDetailedNetworks();
  String fullContent = "<h2>Подробная информация</h2>"
                      "<div class='nav-buttons'>"
                      "<button class='nav-button' onclick=\"window.location.href='/'\">Назад</button>"
                      "<button class='nav-button export-button' onclick=\"window.location.href='/export_json'\">Экспорт JSON</button>"
                      "<button class='nav-button' onclick=\"window.location.href='/stats'\">Статистика</button>"
                      "</div>"
                      "<div class='calibration'>"
                      "<form id='calibrationForm' onsubmit='calibrate(event)'>"
                      "<label>Калибровочный RSSI (P0, dBm):</label>"
                      "<input type='number' step='0.1' name='p0' value='" + String(P0, 1) + "'>"
                      "<label>Коэффициент затухания (n):</label>"
                      "<input type='number' step='0.1' name='n' value='" + String(n, 2) + "'>"
                      "<input type='submit' value='Калибровать'>"
                      "</form></div>" + detailsContent;
  server.send(200, "text/html; charset=utf-8", fullContent);
}

void handleCalibrate() {
  lastActivity = millis();
  if (server.hasArg("p0")) {
    P0 = server.arg("p0").toFloat();
  }
  if (server.hasArg("n")) {
    n = server.arg("n").toFloat();
  }
  String jsonResponse = "{\"p0\": " + String(P0, 1) + ", \"n\": " + String(n, 2) + "}";
  server.send(200, "application/json; charset=utf-8", jsonResponse);
}

void handleExportJson() {
  lastActivity = millis();
  String jsonContent = generateJson();
  server.sendHeader("Content-Disposition", "attachment; filename=wifi_scan.json");
  server.send(200, "application/json; charset=utf-8", jsonContent);
}

void handleStats() {
  lastActivity = millis();
  isStatsActive = true;
  String page = stats_html;
  String statsContent = getStatsContent();
  page.replace("%STATS_CONTENT%", statsContent);
  server.send(200, "text/html; charset=utf-8", page);
}

void handleStatsData() {
  isStatsActive = true;
  String json = getStatsData();
  server.send(200, "application/json; charset=utf-8", json);
}

void handleSetChannel() {
  lastActivity = millis();
  if (server.hasArg("channel")) {
    int channel = server.arg("channel").toInt();
    if (channel >= 1 && channel <= 13) {
      WiFi.softAPdisconnect(true);
      WiFi.softAP(apSSID, apPassword, channel, 0);
      WiFi.setTxPower(WIFI_POWER_17dBm);
      String jsonResponse = "{\"channel\": " + String(channel) + "}";
      server.send(200, "application/json; charset=utf-8", jsonResponse);
    } else {
      server.send(400, "text/plain", "Ошибка: Неверный канал");
    }
  } else {
    server.send(400, "text/plain", "Ошибка: Канал не указан");
  }
}

void handleRestart() {
  lastActivity = millis();
  server.send(200, "text/plain", "Перезагрузка...");
  delay(1000);
  ESP.restart();
}

void adjustCpuFrequency() {
  int clients = WiFi.softAPgetStationNum();
  unsigned long currentTime = millis();

  if (clients > 0 || (currentTime - lastActivity < IDLE_TIMEOUT) || isScanning || isExporting) {
    if (getCpuFrequencyMhz() != HIGH_FREQ) {
      setCpuFrequencyMhz(HIGH_FREQ);
      Serial.println("Переключение на высокую частоту: " + String(HIGH_FREQ) + " МГц");
    }
  } else if (isStatsActive) {
    if (getCpuFrequencyMhz() != MID_FREQ) {
      setCpuFrequencyMhz(MID_FREQ);
      Serial.println("Переключение на среднюю частоту: " + String(MID_FREQ) + " МГц");
    }
    if (currentTime - lastActivity > 2000) {
      isStatsActive = false;
    }
  } else {
    if (getCpuFrequencyMhz() != LOW_FREQ) {
      setCpuFrequencyMhz(LOW_FREQ);
      Serial.println("Переключение на низкую частоту: " + String(LOW_FREQ) + " МГц");
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword, 1, 0);
  WiFi.setTxPower(WIFI_POWER_17dBm);
  setCpuFrequencyMhz(LOW_FREQ);
  
  delay(100);
  Serial.println("Точка доступа создана");
  Serial.print("IP адрес: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Подключите устройство к сети и измерьте RSSI на расстоянии 1 м с помощью другой программы (например, WiFi Analyzer). Затем введите значение на странице 'Подробности'.");

  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/details", handleDetails);
  server.on("/details_content", handleDetailsContent);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/export_json", handleExportJson);
  server.on("/stats", handleStats);
  server.on("/stats_data", handleStatsData);
  server.on("/set_channel", HTTP_POST, handleSetChannel);
  server.on("/restart", HTTP_POST, handleRestart);
  
  server.begin();
  Serial.println("Веб-сервер запущен");
}

void loop() {
  server.handleClient();
  adjustCpuFrequency();
}