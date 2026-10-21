#include <Arduino.h>

#include "interfaces.h"

DRAM_ATTR volatile bool obstacle_fired = false;

void IRAM_ATTR isr() {
  obstacle_fired = true;
}

class ObstacleAvoidanceModule : public Controller, public Repeatable {
  // sensor -- obstacle avoidance module: LOW - obstacle detected, HIGH - no obstacle
 private:
  int pin;
  inline static bool one_instance_exists = false;

 public:
  ObstacleAvoidanceModule(int pin, std::function<void()> on_start, std::function<void()> on_stop,
                          std::function<void()> on_fire)
      : Controller(on_start, on_stop, on_fire) {
    this->pin = pin;
  }

  std::string get_name() override {
    return "ObstacleAvoidanceModule";
  }

  bool init_and_check_and_return_is_success_status() override {
    if (one_instance_exists) {  // allow only one instance of this module (see "obstacle_fired")
      Serial.println("ObstacleAvoidanceModule instance already exists");
      return false;
    }
    one_instance_exists = true;

    // 1. check it. we expect the module initially to be connected and no obstacle present. fail if it's not so
    // do pull DOWN first. expected: HIGH. possible cases:
    // +---------------------------+-------------+
    // | Condition                 | GPIO reads  |
    // +---------------------------+-------------+
    // | Connected, no obstacle    | HIGH        |
    // | Connected, obstacle       | LOW         |
    // | Module disconnected       | LOW         |
    // +---------------------------+-------------+
    // then do pull UP. expected: HIGH. possible cases:
    // +---------------------------+-------------+
    // | Condition                 | GPIO reads  |
    // +---------------------------+-------------+
    // | Connected, no obstacle    | HIGH        |
    // | Connected, obstacle       | LOW         |
    // | Module disconnected       | HIGH        |
    // +---------------------------+-------------+
    int expected = HIGH;
    pinMode(pin, INPUT_PULLDOWN);
    int sensor_state = digitalRead(pin);
    if (sensor_state != expected) {
      Serial.println("Obstacle avoidance module check failed (PULLDOWN)");
      return false;
    }
    pinMode(pin, INPUT_PULLUP);
    sensor_state = digitalRead(pin);
    if (sensor_state != expected) {
      Serial.println("Obstacle avoidance module check failed (PULLUP)");
      return false;
    }

    // 2. initialize the module -- do pull UP. to be HIGH when no module is connected or when no obstacle is present.
    // so the obstacle avoidance module will read LOW when an obstacle is detected, and HIGH otherwise.
    pinMode(pin, INPUT_PULLUP);  // for initialization ignore check code, it can change
    attachInterrupt(digitalPinToInterrupt(pin), isr, FALLING);
    return true;
  }

  void repeat() override {
    if (obstacle_fired) {
      obstacle_fired = false;
      on_fire();
    }
  }
};