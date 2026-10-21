#ifndef INTERFACES_H
#define INTERFACES_H

#include <string>

enum class State {
  SAFE,
  ARM,
  FIRE,
  UNRECOVERABLE  // like SAFE but without an ability to transition back to any other state
};

std::string get_state_name(State state) {
  switch (state) {
    case State::SAFE:
      return "SAFE";
    case State::ARM:
      return "ARM";
    case State::FIRE:
      return "FIRE";
    case State::UNRECOVERABLE:
      return "UNRECOVERABLE";
  }
  return "Unknown";
}

struct StateDetails {
  State state;
  std::string message;
  bool is_arm_timer_active;
};

enum class Event { DO_START, DO_STOP, DO_FIRE, FAILURE };

class Repeatable;
class Display;

class Verifiable {
 public:
  virtual std::string get_name() = 0;
  virtual bool init_and_check_and_return_is_success_status() = 0;

  virtual Repeatable* as_repeatable() {
    return nullptr;
  }

  virtual Display* as_display() {
    return nullptr;
  }
};

class Repeatable : public virtual Verifiable {
 public:
  virtual void repeat() = 0;

  Repeatable* as_repeatable() override {
    return this;
  }
};

class Display : public virtual Verifiable {
 protected:
  std::function<StateDetails()> get_current_state_details;

 public:
  Display(std::function<StateDetails()> get_current_state_details) {
    this->get_current_state_details = get_current_state_details;
  }

  virtual void handle_state_change() = 0;

  Display* as_display() override {
    return this;
  }
};

class Controller : public virtual Verifiable {
 protected:
  std::function<void()> do_start;
  std::function<void()> do_stop;
  std::function<void()> do_fire;

 public:
  Controller(std::function<void()> do_start, std::function<void()> do_stop, std::function<void()> do_fire) {
    this->do_start = do_start;
    this->do_stop = do_stop;
    this->do_fire = do_fire;
  }
};

#endif  // INTERFACES_H
