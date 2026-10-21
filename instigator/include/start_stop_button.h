#include <Arduino.h>

#include "interfaces.h"

DRAM_ATTR volatile bool start_button_pressed = false;
DRAM_ATTR volatile bool stop_button_pressed = false;

void IRAM_ATTR isr_start_button() {
  start_button_pressed = true;
}

void IRAM_ATTR isr_stop_button() {
  stop_button_pressed = true;
}

class StartStopButton : public Controller, public Repeatable {
  // sensor -- obstacle avoidance module: LOW - obstacle detected, HIGH - no obstacle
 private:
  int start_button_pin;
  int stop_button_pin;
  inline static bool one_instance_exists = false;

 public:
  StartStopButton(int start_button_pin, int stop_button_pin, std::function<void()> do_start,
                  std::function<void()> do_stop, std::function<void()> do_fire)
      : Controller(do_start, do_stop, do_fire) {
    this->start_button_pin = start_button_pin;
    this->stop_button_pin = stop_button_pin;
  }

  std::string get_name() override {
    return "StartStopButton";
  }

  bool init_and_check_and_return_is_success_status() override {
    if (one_instance_exists) {  // allow only one instance of this module
      Serial.println("StartStopButton instance already exists");
      return false;
    }
    one_instance_exists = true;

    pinMode(start_button_pin, INPUT_PULLDOWN);
    pinMode(stop_button_pin, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(start_button_pin), isr_start_button, RISING);
    attachInterrupt(digitalPinToInterrupt(stop_button_pin), isr_stop_button, RISING);

    return true;
  }

  void repeat() override {
    if (stop_button_pressed) {
      start_button_pressed = false;
      stop_button_pressed = false;
      do_stop();
    } else if (start_button_pressed) {
      start_button_pressed = false;
      do_start();
    }
  }
};