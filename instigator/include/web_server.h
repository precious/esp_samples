#include <WebServer.h>
#include <WiFi.h>

#include <iomanip>
#include <iostream>
#include <string>

#include "interfaces.h"

std::string MAIN_PAGE_CONTENT = R"|(
<script>
function handleClick(action) {
  let error_message_elem = document.getElementById('error_message');
  fetch('/' + action)
    .then(response => {
      error_message_elem.textContent = (!response.ok) ? 'Error: ' + response.statusText : '';
      if (response.ok) {
        location.reload(); // reload page
      }
    })
    .catch(error => error_message_elem.textContent = 'Error: ' + error.message);
}

let lastUpdatedSeconds = 0;
setInterval(() => {
  lastUpdatedSeconds += 1;
  document.getElementById('last_updated').textContent = 'last page update: ' + lastUpdatedSeconds + ' seconds ago';
}, 1000);
setTimeout(() => location.reload(), 5000);
</script>

<h1>Instigator Web UI</h1><br>

<button type="button" onclick="handleClick('start')">Start</button>&nbsp;&nbsp;&nbsp;
<button type="button" onclick="handleClick('stop')">Stop</button>&nbsp;&nbsp;&nbsp;
<button type="button" onclick="handleClick('fire')">Fire</button><br><br>

<div id="error_message"></div><br>
<div id="last_updated">last updated 0 seconds ago</div><br>
)|";

class SimpleWebServer : public Controller, public Display, public Repeatable {
  WebServer server;

  bool _is_web_server_reachable() {
    WiFiClient client;
    if (!client.connect(WiFi.softAPIP(), 80)) {
      return false;
    }

    client.print("GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    const unsigned long start_time = millis();
    while (client.connected() && !client.available() && millis() - start_time < 1000) {
      server.handleClient();
      delay(1);
    }

    bool reachable = false;
    if (client.available()) {
      String status_line = client.readStringUntil('\n');
      reachable = status_line.startsWith("HTTP/1.") && status_line.indexOf(" 200 ") >= 0;
    }
    client.stop();
    return reachable;
  }

  void _handleHome() {
    std::string content = MAIN_PAGE_CONTENT + "<div>Current state: <b>" + get_state_name(get_current_state()) +
                          "</b> (" + get_current_state_message() + ")</div><br>";
    server.send(200, "text/html", content.c_str());
  }

  void _handleStart() {
    on_start();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }

  void _handleStop() {
    on_stop();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }

  void _handleFire() {
    on_fire();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }

 public:
  SimpleWebServer(std::function<void()> on_start, std::function<void()> on_stop, std::function<void()> on_fire,
                  std::function<State()> get_current_state, std::function<std::string()> get_current_state_message)
      : Controller(on_start, on_stop, on_fire), Display(get_current_state, get_current_state_message), server(80) {
  }

  std::string get_name() override {
    return "SimpleWebServer";
  }

  void repeat() override {
    server.handleClient();  // must run repeatedly
  }

  bool init_and_check_and_return_is_success_status() override {
    server.on("/", [this]() { _handleHome(); });
    server.on("/start", [this]() { _handleStart(); });
    server.on("/stop", [this]() { _handleStop(); });
    server.on("/fire", [this]() { _handleFire(); });
    server.begin();

    return _is_web_server_reachable();
  }

  void handle_state_change() override {
    // nothing to do here
  };
};