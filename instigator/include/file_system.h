#include <Arduino.h>
#include <LittleFS.h>

#include "interfaces.h"

class FileSystem : public Verifiable {
 private:
  int min_required_free_bytes;

 public:
  FileSystem(int min_required_free_bytes) {
    this->min_required_free_bytes = min_required_free_bytes;
  }

  std::string get_name() override {
    return "FileSystem";
  }

  bool init_and_check_and_return_is_success_status() override {
    // check if the filesystem can be mounted and is writable and readable and has enough free space
    if (!LittleFS.begin(false)) {  // we are in production, do not format automatically
      Serial.println("Filesystem mount failed");
      return false;
    }

    if (LittleFS.totalBytes() - LittleFS.usedBytes() < min_required_free_bytes) {
      return false;
    }

    File f = LittleFS.open("/check.txt", FILE_WRITE);
    if (!f) {
      return false;
    }
    f.close();

    f = LittleFS.open("/check.txt", FILE_READ);
    if (!f) {
      return false;
    }
    f.readString();
    f.close();

    return true;
  }
};