#include <Arduino.h>
#include <WiFi.h>

#include "interfaces.h"

class WiFiAccessPoint : public Verifiable {
 private:
  char* ssid;
  char* password;

 public:
  WiFiAccessPoint(char* ssid, char* password) {
    this->ssid = ssid;
    this->password = password;
  }

  std::string get_name() override {
    return "WiFiAccessPoint";
  }

  bool init_and_check_and_return_is_success_status() override {
    bool is_success_status = WiFi.softAP(ssid, password);
    if (is_success_status) {
      Serial.printf("Open: http://%s\n", WiFi.softAPIP().toString().c_str());  // Usually 192.168.4.1
    }
    return is_success_status;
  }
};