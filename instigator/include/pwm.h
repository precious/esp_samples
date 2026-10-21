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

      if (width >= 1000 - 50 && width <= 1000 + 50) {
        do_start();
      } else if (width >= 1500 - 50 && width <= 1500 + 50) {
        do_stop();
      } else if (width >= 2000 - 50 && width <= 2000 + 50) {
        do_fire();
      }
    }
  }
};