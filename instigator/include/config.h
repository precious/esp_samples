#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "interfaces.h"

const char CONFIG_FILE_PATH[] = "/config.json";

class ConfigManager : public Repeatable {
  // reads commands from serial, handles them: to ls files, to cat config, to store config, and to show help
  bool serial_initialized = false;
  bool skipping_until_eol = false;
  std::string buffer;

  void _handle_command(const std::string& command) {
    if (command == "ls") {
      File root = LittleFS.open("/");
      for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        Serial.printf("%s  %d bytes\n", f.name(), f.size());
        f.close();
      }
      root.close();
    } else if (command == "get_config") {
      // if "config.json" exists - print its contents
      if (LittleFS.exists(CONFIG_FILE_PATH)) {
        File f = LittleFS.open(CONFIG_FILE_PATH, FILE_READ);
        if (f) {
          while (f.available()) {
            Serial.write(f.read());
          }
          f.close();
        } else {
          Serial.printf("Failed to open %s for reading\n", CONFIG_FILE_PATH);
        }
      } else {
        Serial.printf("%s does not exist\n", CONFIG_FILE_PATH);
      }
      Serial.println();
    } else if (command.rfind("set_config", 0) == 0) {
      auto json_content = command.substr(std::string("set_config").size());
      if (json_content.size() < 3) {
        Serial.println("Invalid JSON content");
        return;
      }
      File f = LittleFS.open(CONFIG_FILE_PATH, FILE_WRITE);
      if (f) {
        f.print(json_content.c_str());
        f.close();
      } else {
        Serial.printf("Failed to open %s for writing\n", CONFIG_FILE_PATH);
      }
    } else if (command == "help") {
      Serial.println("Available commands: ls, get_config, set_config <json_content>, help");
    } else {
      Serial.println("Unknown command: " + String(command.c_str()));
    }
  }

 public:
  ConfigManager(bool serial_initialized) {
    this->serial_initialized = serial_initialized;
  }

  std::string get_name() override {
    return "ConfigManager";
  }

  bool init_and_check_and_return_is_success_status() override {
    if (!serial_initialized) {
      Serial.begin(115200);
      serial_initialized = true;
    }
    if (!LittleFS.begin(false)) {  // we are in production, do not format automatically
      Serial.println("Filesystem mount failed");
      return false;
    }
    return true;
  }

  void repeat() override {
    // non-blocking read from Serial
    int num_available = Serial.available();
    if (num_available > 0) {
      char ch;
      for (int i = 0; i < num_available; i++) {
        if (!Serial.readBytes(&ch, 1)) {
          break;  // should not happen, just in case
        }
        if (ch == '\n' || ch == '\r' || ch == '\0') {
          skipping_until_eol = false;
          if (buffer.size() > 0) {
            _handle_command(buffer);
            buffer.clear();
          }
          continue;
        }
        if (buffer.size() > 256) {
          Serial.println("Skipping too long command");
          skipping_until_eol = true;
          buffer.clear();
        }
        if (!skipping_until_eol) {
          buffer.push_back(ch);
        }
      }
    }
  }
};

struct Config {
  // all settings must have default values
  int seconds_before_arm = 5 * 60;  // 5 minutes

  void print() const {
    Serial.printf("seconds_before_arm: %d\n", seconds_before_arm);
  }
};

class ConfigReader : public Verifiable {
  Config config;
  bool write_default_if_not_exists;

  void _write_config_to_file(File f) {
    JsonDocument doc;
    doc["seconds_before_arm"] = config.seconds_before_arm;
    Serial.print("Minified: ");
    serializeJson(doc, f);
  }

 public:
  ConfigReader(bool write_default_if_not_exists) {
    this->write_default_if_not_exists = write_default_if_not_exists;
  }

  std::string get_name() override {
    return "ConfigReader";
  }

  bool init_and_check_and_return_is_success_status() override {
    if (!LittleFS.begin(false)) {
      Serial.println("Filesystem mount failed");
      return false;
    }
    if (!LittleFS.exists(CONFIG_FILE_PATH)) {
      if (write_default_if_not_exists) {
        File f = LittleFS.open(CONFIG_FILE_PATH, FILE_WRITE);
        if (!f) {
          Serial.printf("Failed to create %s\n", CONFIG_FILE_PATH);
          return false;
        }
        _write_config_to_file(f);
        f.close();
      } else {
        Serial.printf("%s does not exist\n", CONFIG_FILE_PATH);
        return false;
      }
    }
    File f = LittleFS.open(CONFIG_FILE_PATH, FILE_READ);
    if (!f) {
      Serial.printf("Failed to open %s for reading\n", CONFIG_FILE_PATH);
      return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return false;
    }

    if (doc.containsKey("seconds_before_arm") && doc["seconds_before_arm"].is<int>()) {
      config.seconds_before_arm = doc["seconds_before_arm"];
    } else {
      Serial.println("seconds_before_arm not found or not an integer");
      return false;
    }
    return true;
  }

  Config get_config() const {
    return config;
  }
};
