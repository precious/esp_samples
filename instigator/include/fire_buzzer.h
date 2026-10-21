#include <Arduino.h>

#include "interfaces.h"

class FireBuzzer : public Display {
 private:
  int pin;

 public:
  FireBuzzer(int pin, std::function<State()> get_current_state, std::function<std::string()> get_current_state_message)
      : Display(get_current_state, get_current_state_message) {
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
    if (get_current_state() == State::FIRE) {
      fire();
    }
  }
};