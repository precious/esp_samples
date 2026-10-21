#include <Arduino.h>

#include "interfaces.h"

class FireBuzzer : public Display {
 private:
  int pin;

 public:
  FireBuzzer(int pin, std::function<StateDetails()> get_current_state_details) : Display(get_current_state_details) {
    this->pin = pin;
  }

  std::string get_name() override {
    return "FireBuzzer";
  }

  bool init_and_check_and_return_is_success_status() override {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    return true;
  }

  void fire() {
    digitalWrite(pin, HIGH);
  }

  void handle_state_change() override {
    if (get_current_state_details().state == State::FIRE) {
      fire();
    }
  }
};