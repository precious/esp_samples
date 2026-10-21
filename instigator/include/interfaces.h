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

  // // return `this` when the object also supports repeatable.
  // virtual Repeatable* as_repeatable() {
  //   return nullptr;
  // }

  // // return `this` when the object also supports display.
  // virtual Display* as_display() {
  //   return nullptr;
  // }
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
  std::function<State()> get_current_state;
  std::function<std::string()> get_current_state_message;

 public:
  Display(std::function<State()> get_current_state, std::function<std::string()> get_current_state_message) {
    this->get_current_state = get_current_state;
    this->get_current_state_message = get_current_state_message;
  }

  virtual void handle_state_change() = 0;

  Display* as_display() override {
    return this;
  }
};

class Controller : public virtual Verifiable {
 protected:
  std::function<void()> on_start;
  std::function<void()> on_stop;
  std::function<void()> on_fire;

 public:
  Controller(std::function<void()> on_start, std::function<void()> on_stop, std::function<void()> on_fire) {
    this->on_start = on_start;
    this->on_stop = on_stop;
    this->on_fire = on_fire;
  }
};

#endif  // INTERFACES_H
