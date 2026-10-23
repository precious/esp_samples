#include <Arduino.h>
#include <LittleFS.h>
#include <mavlink.h>

#include "mavlink_telemetry_csv.h"
#include "mavlink_telemetry_formatters.h"

#define FLUSH_INTERVAL_MS 100
#define LOG_FILE_MAX_SIZE_BYTES 128 * 1024
#define MAVLINK_RX_PIN 4
#define MAVLINK_TX_PIN 25

class _StreamsManager {
  // handle log file rotation
  inline static const char LOG_FILE_PATH[] = "/mavlink_telemetry_log.csv";
  inline static const char LOG_FILE_PATH_1[] = "/mavlink_telemetry_log.1.csv";
  File log_file;

 public:
  _StreamsManager() {
    bool ok_to_write =
        LittleFS.begin(true) && (LittleFS.totalBytes() - LittleFS.usedBytes() > LOG_FILE_MAX_SIZE_BYTES * 3);
    if (!ok_to_write) {
      Serial.println("LittleFS mount failed or it has not enough space");
    } else {
      log_file = LittleFS.open(LOG_FILE_PATH, "a");
    }
  }

  std::vector<Stream*> get_streams() {
    std::vector<Stream*> streams = {&Serial};
    if (log_file) {
      if (log_file.size() >= LOG_FILE_MAX_SIZE_BYTES) {  // rotate
        log_file.close();
        if (LittleFS.rename(LOG_FILE_PATH, LOG_FILE_PATH_1)) {
          log_file = LittleFS.open(LOG_FILE_PATH, "w");
        } else {
          Serial.println("Failed to rename old log file. Logging stopped.");
          log_file = File();
        }
      }
      if (log_file) {
        streams.push_back(&log_file);
      }
    }
    return streams;
  }
};

StreamingMAVLinkTelemetryCSVWriter csv_writer(formatters, formatters_map);
_StreamsManager* streams_manager;

void setup() {
  Serial.begin(115200);
  Serial2.begin(57600, SERIAL_8N1, MAVLINK_RX_PIN, MAVLINK_TX_PIN);

  streams_manager = new _StreamsManager();
  csv_writer.write_header(streams_manager->get_streams());
}

void loop() {
  static unsigned long last_flush_time = 0;

  while (Serial2.available() > 0) {
    const uint8_t c = Serial2.read();
    mavlink_message_t msg;
    mavlink_status_t status;
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      csv_writer.queue(msg);
    }
  }

  if (millis() - last_flush_time >= FLUSH_INTERVAL_MS) {
    csv_writer.flush(streams_manager->get_streams());
    last_flush_time = millis();
  }
}