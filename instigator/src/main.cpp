#include <Arduino.h>
#include <LittleFS.h>

#include <iostream>
#include <string>

#include "config.h"
#include "file_system.h"
#include "fire_buzzer.h"
#include "obstacle_avoidance_module.h"
#include "pwm.h"
#include "rgb_state_led.h"
#include "start_stop_button.h"
#include "state_machine.h"
#include "web_server.h"
#include "wifi_access_point.h"

#define FIRE_BUZZER_PIN 22
#define OBSTACLE_AVOIDANCE_MODULE_PIN 23

#define RGB_STATE_LED_RED_PIN 18
#define RGB_STATE_LED_GREEN_PIN 19
#define RGB_STATE_LED_BLUE_PIN 21

#define START_BUTTON_PIN 2
#define STOP_BUTTON_PIN 4

#define PWM_PIN 5

const char AP_SSID[] = "InstigatorAP";
const char AP_PASSWORD[] = SECRET_WIFI_PASS;
static_assert(sizeof(AP_PASSWORD) > 1, "WiFi password cannot be empty!");

StateMachine* state_machine;
std::vector<Repeatable*> repeatable_modules;

void setup() {
  Serial.begin(115200);

  bool broken_config = false;
  ConfigReader* config_reader = new ConfigReader(true);
  if (!config_reader->init_and_check_and_return_is_success_status()) {
    Serial.println("ConfigReader initialization failed");
    broken_config = true;
  }

  state_machine = new StateMachine(config_reader->get_config().seconds_before_arm);
  if (broken_config) {
    state_machine->handle_event(Event::FAILURE, "Broken config detected");
  }

  auto do_start = []() { state_machine->handle_event(Event::DO_START, ""); };
  auto do_stop = []() { state_machine->handle_event(Event::DO_STOP, ""); };
  auto do_fire = []() { state_machine->handle_event(Event::DO_FIRE, ""); };
  auto get_current_state_details = []() { return state_machine->get_current_state_details(); };

  // create modules -- all modules are verifiable
  std::vector<Verifiable*> all_modules{
      new ObstacleAvoidanceModule(OBSTACLE_AVOIDANCE_MODULE_PIN, do_start, do_stop, do_fire),
      new StartStopButton(START_BUTTON_PIN, STOP_BUTTON_PIN, do_start, do_stop, do_fire),
      new PWMController(PWM_PIN, do_start, do_stop, do_fire),
      new FireBuzzer(FIRE_BUZZER_PIN, get_current_state_details),
      new FileSystem(1024 * 64 * 4),  //
      new ConfigManager(true),
      new WiFiAccessPoint((char*)AP_SSID, (char*)AP_PASSWORD),
      new SimpleWebServer(do_start, do_stop, do_fire, get_current_state_details),
      new RGBStateLED(RGB_STATE_LED_RED_PIN, RGB_STATE_LED_GREEN_PIN, RGB_STATE_LED_BLUE_PIN,
                      get_current_state_details)};

  // initialize and check all modules
  for (Verifiable* module : all_modules) {
    if (!module->init_and_check_and_return_is_success_status()) {
      Serial.println((module->get_name() + " initialization failed").c_str());
      state_machine->handle_event(Event::FAILURE, (module->get_name() + " initialization failed").c_str());
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
      state_machine->add_on_state_or_message_change_callback(
          [module]() { module->as_display()->handle_state_change(); });
    }
  }

  // set once again the initial status to safe to ensure all modules are aware of it
  state_machine->handle_event(Event::DO_STOP, "Initialized");
}

void loop() {
  for (Repeatable* module : repeatable_modules) {
    module->repeat();
  }
}
