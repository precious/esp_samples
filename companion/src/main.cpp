#include <Arduino.h>
#include <LittleFS.h>
#include <mavlink.h>

#include "mavlink_telemetry_csv.h"
#include "mavlink_telemetry_formatters.h"

#define FLUSH_INTERVAL_MS 100
#define MAVLINK_RX_PIN 4
#define MAVLINK_TX_PIN 25

void setup() {
  Serial.begin(115200);
  Serial2.begin(57600, SERIAL_8N1, MAVLINK_RX_PIN, MAVLINK_TX_PIN);
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }
  Serial.printf("Total: %u bytes\n", LittleFS.totalBytes());
  Serial.printf("Used:  %u bytes\n", LittleFS.usedBytes());
  Serial.printf("Free:  %u bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
}

void loop() {
  // static const int max_fields = 16;
  // static MessageField fields[max_fields];
  static bool first_run = true;
  static unsigned long last_flush_time = 0;
  static std::vector<Stream*> streams = {&Serial};
  static StreamingMAVLinkTelemetryCSVWriter csv_writer(formatters, formatters_map);
  if (first_run) {
    first_run = false;
    csv_writer.write_header(streams);
  }

  while (Serial2.available() > 0) {
    const uint8_t c = Serial2.read();
    mavlink_message_t msg;
    mavlink_status_t status;
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      csv_writer.queue(msg);
    }
  }

  if (millis() - last_flush_time >= FLUSH_INTERVAL_MS) {
    csv_writer.flush(streams);
    last_flush_time = millis();
  }
}
