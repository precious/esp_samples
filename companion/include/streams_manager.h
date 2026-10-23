#include <Arduino.h>
#include <LittleFS.h>

#include <functional>
#include <vector>

class StreamsManager {
  // write to Serial and optionally to a log file; handle log file rotation
  bool _first_call = true;
  int log_file_max_size_bytes;
  File log_file;

 public:
  inline static const char LOG_FILE_PATH[] = "/mavlink_telemetry_log.csv";
  inline static const char LOG_FILE_PATH_1[] = "/mavlink_telemetry_log.1.csv";

  StreamsManager(int log_file_max_size_bytes_) : log_file_max_size_bytes(log_file_max_size_bytes_) {}

  std::vector<Stream*> get_streams(std::function<int(Stream*)> initializer) {
    std::vector<Stream*> streams = {&Serial};

    if (_first_call) {
      _first_call = false;
      initializer(&Serial);

      bool ok_to_write =
          LittleFS.begin(true) && (LittleFS.totalBytes() - LittleFS.usedBytes() > log_file_max_size_bytes * 3);
      if (!ok_to_write) {
        Serial.println("LittleFS mount failed or it has not enough space");
      } else {
        log_file = LittleFS.open(LOG_FILE_PATH, "a");
        if (log_file && log_file.size() == 0) {
          initializer(&log_file);
        }
      }
    }

    if (log_file) {
      if (log_file.size() >= log_file_max_size_bytes) {  // rotate
        log_file.close();
        if (LittleFS.rename(LOG_FILE_PATH, LOG_FILE_PATH_1)) {
          log_file = LittleFS.open(LOG_FILE_PATH, "w");
          initializer(&log_file);
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
