#include <Arduino.h>

#include "interfaces.h"

volatile uint32_t pulseStart = 0;
volatile uint32_t pulseWidth = 0;
volatile bool pulseReady = false;
int pwm_pin;

void IRAM_ATTR on_pwm_change() {
  uint32_t now = micros();

  if (digitalRead(pwm_pin)) {
    pulseStart = now;  // Rising edge
  } else {
    pulseWidth = now - pulseStart;  // Falling edge
    pulseReady = true;
  }
}

class PWMController : public Repeatable, public Controller {
  // 1000 us command = start
  // 1500 us command = stop
  // 2000 us command = fire
  inline static bool one_instance_exists = false;

  enum class Command { NONE, START, STOP, FIRE };

  Command last_command = Command::NONE;
  uint8_t matching_command_frames = 0;

 public:
  PWMController(int pin, std::function<void()> do_start, std::function<void()> do_stop, std::function<void()> do_fire)
      : Controller(do_start, do_stop, do_fire) {
    pwm_pin = pin;  // set the global pwm_pin
  }

  std::string get_name() override {
    return "PWMController";
  }

  bool init_and_check_and_return_is_success_status() override {
    if (one_instance_exists) {  // allow only one instance of this module (see "pwm_pin" global var)
      Serial.println("PWMController instance already exists");
      return false;
    }
    one_instance_exists = true;

    pinMode(pwm_pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(pwm_pin), on_pwm_change, CHANGE);
    return true;
  }

  void repeat() override {
    if (pulseReady) {
      noInterrupts();
      uint32_t width = pulseWidth;
      pulseReady = false;
      interrupts();

      Command command = Command::NONE;
      if (width >= 1000 - 50 && width <= 1000 + 50) {
        command = Command::START;
      } else if (width >= 1500 - 50 && width <= 1500 + 50) {
        command = Command::STOP;
      } else if (width >= 2000 - 50 && width <= 2000 + 50) {
        command = Command::FIRE;
      }

      // a command requires two consecutive valid, matching PWM frames before it is dispatched, and is dispatched
      // once per command change. This rejects a one-frame glitch without adding signal-loss behavior or changing
      // terminal FIRE semantics
      if (command == Command::NONE) {
        last_command = Command::NONE;
        matching_command_frames = 0;
        return;
      }

      if (command != last_command) {
        last_command = command;
        matching_command_frames = 1;
        return;
      }

      if (matching_command_frames != 1) {
        return;
      }
      matching_command_frames = 2;

      if (command == Command::START) {
        do_start();
      } else if (command == Command::STOP) {
        do_stop();
      } else {
        do_fire();
      }
    }
  }
};
