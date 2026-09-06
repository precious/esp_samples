#include <Arduino.h>
#include <esp_task_wdt.h>

#define BUTTON_PIN 4
#define LED_PIN 2
#define DOUBLE_PRESS_THRESHOLD_MS 500
#define BUTTON_DEBOUNCE_MS 100

#define BUTTON_RAW_PRESS_QUEUE_SIZE 50
#define LED_BLINKING_INTERVAL_QUEUE_SIZE 50

#define LED_BLINKING_MULTIPLIER_MS 250
#define LED_BLINKING_MAX_POWER_OF_2 4  // led blinking interval = 2^CURRENT_POWER_OF_2 * MULTIPLIER

QueueHandle_t BUTTON_RAW_PRESS_QUEUE = NULL;
QueueHandle_t LED_BLINKING_INTERVAL_QUEUE = NULL;

SemaphoreHandle_t MUTEX_A = NULL;
SemaphoreHandle_t MUTEX_B = NULL;

void IRAM_ATTR button_pressed() {
  static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  static unsigned long last_button_pressed_ms = 0;
  unsigned long ms = millis();
  if (ms - last_button_pressed_ms > BUTTON_DEBOUNCE_MS) {
    last_button_pressed_ms = ms;
    xQueueSendFromISR(BUTTON_RAW_PRESS_QUEUE, &ms, &xHigherPriorityTaskWoken);
  };
}

void button_task(void* parameter) {
  static unsigned char current_power_of_2 = 0;
  unsigned long ms;
  for (;;) {
    if (xQueueReceive(BUTTON_RAW_PRESS_QUEUE, &ms, portMAX_DELAY)) {
      if (xQueueReceive(BUTTON_RAW_PRESS_QUEUE, &ms, DOUBLE_PRESS_THRESHOLD_MS / portTICK_PERIOD_MS)) {
        // double click -- slow-down
        current_power_of_2 = (current_power_of_2 > 0) ? current_power_of_2 - 1 : LED_BLINKING_MAX_POWER_OF_2;
      } else {
        // single click -- speed-up
        current_power_of_2 = (current_power_of_2 < LED_BLINKING_MAX_POWER_OF_2) ? current_power_of_2 + 1 : 0;
      }
      int32_t led_blinking_interval = (1 << current_power_of_2) * LED_BLINKING_MULTIPLIER_MS;
      xQueueSend(LED_BLINKING_INTERVAL_QUEUE, &led_blinking_interval, portMAX_DELAY);
    }
  }
}

void led_task(void* parameter) {
  int32_t led_blinking_interval = LED_BLINKING_MULTIPLIER_MS;
  for (;;) {
    assert(led_blinking_interval > 0);
    digitalWrite(LED_PIN, HIGH);  // turn LED on
    bool interval_changed = false;
    for (int32_t i = -led_blinking_interval; i < led_blinking_interval; i += LED_BLINKING_MULTIPLIER_MS) {
      while (xQueueReceive(LED_BLINKING_INTERVAL_QUEUE, &led_blinking_interval, 0) == pdPASS) interval_changed = true;
      if (interval_changed) break;
      if (i >= 0 && i - LED_BLINKING_MULTIPLIER_MS < 0) digitalWrite(LED_PIN, LOW);  // sign changed, turn LED off
      vTaskDelay(LED_BLINKING_MULTIPLIER_MS / portTICK_PERIOD_MS);
    }
  }
}

void mutex_task(void* parameter) {
  // depending on parameter ("A" or "B") - take mutex A first, then B OR B first, then A. then release both
  char mutex_to_take_first = ((char*)parameter)[0];  // should be either "A" or "B"
  assert(mutex_to_take_first == 'A' or mutex_to_take_first == 'B');
  SemaphoreHandle_t first_mutex, second_mutex;
  if (mutex_to_take_first == 'A') {
    first_mutex = MUTEX_A;
    second_mutex = MUTEX_B;
  } else {
    first_mutex = MUTEX_B;
    second_mutex = MUTEX_A;
  }

  while (1) {
    Serial.printf("Task mutex %c first: initialized, sleep for 3 seconds\n", mutex_to_take_first);
    vTaskDelay(3000 / portTICK_PERIOD_MS);  // sleep for 3 second first
    Serial.printf("Task mutex %c first: take first mutex\n", mutex_to_take_first);
    BaseType_t ret_val = xSemaphoreTake(first_mutex, UINT32_MAX);  // take 1st mutex, wait indefinitely
    assert(ret_val == pdTRUE);
    Serial.printf("Task mutex %c first: first mutex taken, sleep for 2 seconds\n", mutex_to_take_first);
    vTaskDelay(2000 / portTICK_PERIOD_MS);  // sleep for 2 seconds
    Serial.printf("Task mutex %c first: take second mutex\n", mutex_to_take_first);
    ret_val = xSemaphoreTake(second_mutex, UINT32_MAX);  // take 2nd mutex, wait indefinitely
    assert(ret_val == pdTRUE);
    Serial.printf("Task mutex %c first: second mutex taken, sleep for 1 second\n", mutex_to_take_first);
    vTaskDelay(1000 / portTICK_PERIOD_MS);  // sleep for 1 second
    Serial.printf("Task mutex %c first: release both mutexes\n", mutex_to_take_first);
    ret_val = xSemaphoreGive(second_mutex);  // release 2nd mutex
    assert(ret_val == pdTRUE);
    ret_val = xSemaphoreGive(first_mutex);  // release 1st mutex
    assert(ret_val == pdTRUE);
    Serial.printf("Task mutex %c first: both mutexes release, exit task\n", mutex_to_take_first);
    esp_task_wdt_reset();
  }
}

void setup() {
  Serial.begin(115200);

  BUTTON_RAW_PRESS_QUEUE = xQueueCreate(BUTTON_RAW_PRESS_QUEUE_SIZE, sizeof(unsigned long));
  if (BUTTON_RAW_PRESS_QUEUE == NULL) {
    Serial.println("Failed to create queue (raw button)");
    while (1);
  }

  LED_BLINKING_INTERVAL_QUEUE = xQueueCreate(LED_BLINKING_INTERVAL_QUEUE_SIZE, sizeof(int32_t));
  if (LED_BLINKING_INTERVAL_QUEUE == NULL) {
    Serial.println("Failed to create queue (button)");
    while (1);
  }

  xTaskCreatePinnedToCore(button_task, "button_task", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(led_task, "led_task", 4096, NULL, 1, NULL, 1);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_pressed, FALLING);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // break multitasking - by creating a deadlock
  MUTEX_A = xSemaphoreCreateMutex();
  MUTEX_B = xSemaphoreCreateMutex();
  if (MUTEX_A == NULL or MUTEX_B == NULL) {
    Serial.println("Failed to create mutex");
    while (1);
  }
  TaskHandle_t mutex_a_first_task_handle, mutex_b_first_task_handle;
  xTaskCreatePinnedToCore(mutex_task, "mutex A first", 4096, (void*)"A", 1, &mutex_a_first_task_handle, 0);
  xTaskCreatePinnedToCore(mutex_task, "mutex B first", 4096, (void*)"B", 1, &mutex_b_first_task_handle, 1);

  // init TWTD
  esp_task_wdt_init(10, false);                 // allow up to 10 seconds of task unresponsiveness
  esp_task_wdt_add(mutex_a_first_task_handle);  // monitor "mutex A first" task
  esp_task_wdt_add(mutex_b_first_task_handle);  // monitor "mutex B first" task
}

void loop() {
}
