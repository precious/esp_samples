#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <mavlink.h>

#include "mavlink_telemetry_csv.h"
#include "mavlink_telemetry_formatters.h"
#include "streams_manager.h"

#define FLUSH_INTERVAL_MS 100
#define LOG_FILE_MAX_SIZE_BYTES 128 * 1024
#define MAVLINK_RX_PIN 4
#define MAVLINK_TX_PIN 25

WebServer server(80);
StreamingMAVLinkTelemetryCSVWriter csv_writer(formatters, formatters_map);
StreamsManager streams_manager(LOG_FILE_MAX_SIZE_BYTES);
std::array<const char*, 2> files = {streams_manager.LOG_FILE_PATH, streams_manager.LOG_FILE_PATH_1};

void setup() {
  Serial.begin(115200);
  Serial2.begin(57600, SERIAL_8N1, MAVLINK_RX_PIN, MAVLINK_TX_PIN);

  const char ap_password[] = SECRET_WIFI_PASS;
  static_assert(sizeof(ap_password) > 1, "WiFi password cannot be empty!");
  while (!WiFi.softAP("CompanionAP", ap_password)) {
    Serial.print(".");
    delay(200);
  };
  Serial.printf("Open: http://%s\n", WiFi.softAPIP().toString().c_str());  // Usually 192.168.4.1

  bool fs_available = LittleFS.begin(true);
  server.on("/", HTTP_GET, [fs_available] {
    std::string response = "CompanionAP is running<br>";
    if (fs_available) {
      for (int i = 0; i < files.size(); i++) {
        File file = LittleFS.open(files[i], "r");
        if (file) {
          response +=
              std::string("<a href=\"") + files[i] + "\">" + files[i] + "</a> " + std::to_string(file.size()) + "<br>";
          file.close();
        }
      }
    }
    server.send(200, "text/html", response.c_str());
  });
  server.onNotFound([]() {
    String path = server.uri();
    bool found = std::any_of(files.begin(), files.end(),
                             [&path](const char* file) { return std::string(file) == std::string(path.c_str()); });
    if (found) {
      File f = LittleFS.open(path.c_str(), "r");
      if (f) {
        server.streamFile(f, "text/csv");
        f.close();
        return;
      }
    }
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
}

void loop() {
  static unsigned long last_flush_time = 0;
  static std::function<int(Stream*)> init = [](Stream* s) { return csv_writer.write_header(std::vector<Stream*>{s}); };

  while (Serial2.available() > 0) {
    const uint8_t c = Serial2.read();
    mavlink_message_t msg;
    mavlink_status_t status;
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      csv_writer.queue(msg);
    }
  }

  if (millis() - last_flush_time >= FLUSH_INTERVAL_MS) {
    csv_writer.flush(streams_manager.get_streams(init));
    last_flush_time = millis();
  }

  server.handleClient();  // must run repeatedly
}