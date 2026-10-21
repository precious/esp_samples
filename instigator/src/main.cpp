#include <Arduino.h>
#include <LittleFS.h>

#include <iostream>
#include <string>

#include "file_system.h"
#include "fire_buzzer.h"
#include "obstacle_avoidance_module.h"
#include "state_machine.h"
#include "web_server.h"
#include "wifi_access_point.h"

#define FIRE_BUZZER_PIN 22
#define OBSTACLE_AVOIDANCE_MODULE_PIN 23
#define SECONDS_BEFORE_ARM 10

const char AP_SSID[] = "InstigatorAP";
const char AP_PASSWORD[] = SECRET_WIFI_PASS;
static_assert(sizeof(AP_PASSWORD) > 1, "WiFi password cannot be empty!");

void noop() {
  Serial.println("noop");
}

StateMachine state_machine(SECONDS_BEFORE_ARM);
std::vector<Repeatable*> repeatable_modules;

void setup() {
  Serial.begin(115200);

  auto on_start = []() { state_machine.handle_event(Event::DO_START, ""); };
  auto on_stop = []() { state_machine.handle_event(Event::DO_STOP, ""); };
  auto on_fire = []() { state_machine.handle_event(Event::DO_FIRE, ""); };
  auto get_current_state = []() { return state_machine.get_current_state(); };
  auto get_current_state_message = []() { return state_machine.get_current_state_message(); };

  // create modules -- all modules are verifiable
  std::vector<Verifiable*> all_modules{
      new ObstacleAvoidanceModule(OBSTACLE_AVOIDANCE_MODULE_PIN, on_start, on_stop, on_fire),
      new FireBuzzer(FIRE_BUZZER_PIN, get_current_state, get_current_state_message),
      new FileSystem(1024 * 64 * 4),  //
      new WiFiAccessPoint((char*)AP_SSID, (char*)AP_PASSWORD),
      new SimpleWebServer(on_start, on_stop, on_fire, get_current_state, get_current_state_message)};

  // initialize and check all modules
  for (Verifiable* module : all_modules) {
    if (!module->init_and_check_and_return_is_success_status()) {
      Serial.println((module->get_name() + " initialization failed").c_str());
      state_machine.handle_event(Event::FAILURE, (module->get_name() + " initialization failed").c_str());
      return;
    }
  }

  // collect all repeatable modules
  for (Verifiable* module : all_modules) {
    if (module->as_repeatable()) {
      repeatable_modules.push_back(module->as_repeatable());
    }
  }

  // collect all displays, make state machine call them on state or message change
  for (Verifiable* module : all_modules) {
    if (module->as_display()) {
      state_machine.add_on_state_or_message_change_callback(
          [module]() { module->as_display()->handle_state_change(); });
    }
  }

  Serial.println("Setup complete, num of repeatable modules: " + String(repeatable_modules.size()));
}

void loop() {
  for (Repeatable* module : repeatable_modules) {
    module->repeat();
  }
}
