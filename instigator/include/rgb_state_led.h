#include <Arduino.h>

#include "interfaces.h"

class RGBStateLED : public Display {
 private:
  int red_pin;
  int green_pin;
  int blue_pin;

  void _all_pins_low() {
    digitalWrite(red_pin, LOW);
    digitalWrite(green_pin, LOW);
    digitalWrite(blue_pin, LOW);
  }

 public:
  RGBStateLED(int red_pin, int green_pin, int blue_pin, std::function<StateDetails()> get_current_state_details)
      : Display(get_current_state_details) {
    this->red_pin = red_pin;
    this->green_pin = green_pin;
    this->blue_pin = blue_pin;
  }

  std::string get_name() override {
    return "RGBStateLED";
  }

  bool init_and_check_and_return_is_success_status() override {
    pinMode(red_pin, OUTPUT);
    pinMode(green_pin, OUTPUT);
    pinMode(blue_pin, OUTPUT);
    _all_pins_low();
    return true;
  }

  void handle_state_change() override {
    auto current_state_details = get_current_state_details();
    _all_pins_low();
    switch (current_state_details.state) {
      case State::SAFE:
        if (!current_state_details.is_arm_timer_active) {
          digitalWrite(green_pin, HIGH);
        } else {
          digitalWrite(blue_pin, HIGH);
        }
        break;
      case State::ARM:
      case State::FIRE:
        digitalWrite(red_pin, HIGH);
        break;
    }
  }
};
