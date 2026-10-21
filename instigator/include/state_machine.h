#include <Arduino.h>

#include <string>
#include <vector>

#include "esp_timer.h"
#include "interfaces.h"

class StateMachine {
  State current_state;
  std::string current_state_message;
  int seconds_before_arm;
  int last_arm_timer_start_ms = -1;
  esp_timer_handle_t arm_timer = nullptr;
  std::vector<std::function<void()>> on_state_or_message_change;

  bool _is_arm_timer_active() {
    return esp_timer_is_active(arm_timer);
  }

  void _cancel_arm_timer() {
    if (_is_arm_timer_active()) {
      esp_timer_stop(arm_timer);
    }
  }

  void _restart_arm_timer() {
    _cancel_arm_timer();
    esp_timer_create_args_t args = {};
    args.callback = &_arm_timer_callback_set_state_to_arm;
    args.arg = this;
    args.name = "arm_timer";
    esp_timer_create(&args, &arm_timer);
    esp_timer_start_once(arm_timer, seconds_before_arm * 1000000ULL);
    last_arm_timer_start_ms = millis();
  }

  bool _is_sanity_arm_timer_check_passed() {
    const int safe_gap_ms = 10;  // small buffer to account for timing inaccuracies
    return last_arm_timer_start_ms > 0 &&
           (millis() - last_arm_timer_start_ms) > (seconds_before_arm * 1000 - safe_gap_ms);
  }

  void _set_message(std::string message_internal, std::string message_external) {
    current_state_message = message_internal + " | " + message_external;
    for (const auto& item : on_state_or_message_change) {
      item();
    }
  }

  void _cancel_arm_timer_and_set_state(State new_state, std::string message_internal, std::string message_external) {
    _cancel_arm_timer();  // any set state call cancels timer, even if the new state is the same as the current state
    if (current_state == new_state) {
      _set_message("State unchanged", message_internal + " | " + message_external);
      return;
    }
    if ((new_state == State::ARM || new_state == State::FIRE) && !_is_sanity_arm_timer_check_passed()) {
      // something went wrong
      current_state = State::UNRECOVERABLE;
      _set_message("Sanity check failed", message_internal + " | " + message_external);
      return;
    }
    if (current_state != State::UNRECOVERABLE) {
      current_state = new_state;
      _set_message(message_internal, message_external);
    }
  }

  static void _arm_timer_callback_set_state_to_arm(void* arg) {
    StateMachine* sm = static_cast<StateMachine*>(arg);
    sm->_cancel_arm_timer_and_set_state(State::ARM, "ARM timer expired", "Setting ARM state");
  }

 public:
  StateMachine(int seconds_before_arm)
      : current_state(State::SAFE), current_state_message("SAFE"), seconds_before_arm(seconds_before_arm) {
  }

  State get_current_state() const {
    return current_state;
  }

  std::string get_current_state_message() {
    std::string extra = "";
    if (_is_arm_timer_active()) {
      uint64_t passed_ms = millis() - last_arm_timer_start_ms, remaining_ms = seconds_before_arm * 1000 - passed_ms;
      if (remaining_ms > 0) {
        uint64_t remaining_seconds = remaining_ms / 1000, h = remaining_seconds / 3600,
                 m = (remaining_seconds % 3600) / 60, s = remaining_seconds % 60;
        extra = std::to_string(h) + "h " + std::to_string(m) + "m " + std::to_string(s) + "s";
      }
    }
    return current_state_message + (extra.empty() ? "" : " | " + extra);
  }

  void handle_event(Event event, std::string message) {
    switch (event) {
      case Event::DO_START:
        // if current state is safe and ARM timer is not active, start the ARM timer to transit to arm;
        // otherwise, do nothing
        if (current_state == State::SAFE && !_is_arm_timer_active()) {
          _restart_arm_timer();
          _set_message("ARM Timer started", message);
        } else {
          _set_message("Event ignored", message);
        }
        break;
      case Event::DO_STOP:
        _cancel_arm_timer_and_set_state(State::SAFE, "SAFE", message);  // any s change cancels ARM timer
        break;
      case Event::DO_FIRE:
        if (current_state == State::ARM) {
          _cancel_arm_timer_and_set_state(State::FIRE, "FIRE", message);
        }
        break;
      case Event::FAILURE:
        _cancel_arm_timer_and_set_state(State::UNRECOVERABLE, "FAILURE", message);
        break;
      default:  // should not happen
        _cancel_arm_timer_and_set_state(State::UNRECOVERABLE, "FAILURE - Unknown event", message);
        break;
    }
  }

  void add_on_state_or_message_change_callback(std::function<void()> callback) {
    on_state_or_message_change.push_back(callback);
  }
};
