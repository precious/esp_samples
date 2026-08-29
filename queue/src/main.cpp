#include <Arduino.h>
#include <RadioLib.h>

#define BUTTON_PIN 4
#define DOUBLE_PRESS_THRESHOLD_MS 500
#define BUTTON_DEBOUNCE_MS 100

#define BUTTON_RAW_PRESS_QUEUE_SIZE 20
#define BUTTON_PRESS_QUEUE_SIZE 20
#define RADIO_QUEUE_SIZE 50

#define SPI_LORA_MOSI 27
#define SPI_LORA_MISO 19
#define SPI_LORA_SCK 5
#define SPI_LORA_CS 18
#define SPI_LORA_RST 23
#define SPI_LORA_DIO0 26
#define SPI_LORA_DIO1 33
#define SPI_LORA_DIO2 32

#define LORA_SF 9
#define LORA_BW 125.0
#define LORA_RECEIVE_TIMEOUT_MS 60000  // 1 minute

typedef enum { BUTTON_PRESS_SINGLE, BUTTON_PRESS_DOUBLE } ButtonPressType;

volatile unsigned long LAST_BUTTON_PRESSED_MS = 0;
volatile unsigned long LAST_BUTTON_PRESSED_US = 0;  // microseconds

QueueHandle_t BUTTON_RAW_PRESS_QUEUE = NULL;
QueueHandle_t BUTTON_PRESS_QUEUE = NULL;
QueueHandle_t RADIO_QUEUE = NULL;

SPISettings spiSettings(8000000, MSBFIRST, SPI_MODE0);
Module* mod = new Module(SPI_LORA_CS, SPI_LORA_DIO0, SPI_LORA_RST, SPI_LORA_DIO1, SPI, spiSettings);
SX1276 radio = SX1276(mod);

void IRAM_ATTR button_pressed() {
  unsigned long ms = millis(), delta = ms - LAST_BUTTON_PRESSED_MS;
  if (delta > BUTTON_DEBOUNCE_MS) {
    LAST_BUTTON_PRESSED_MS = ms;
    LAST_BUTTON_PRESSED_US = micros();
    xQueueSend(BUTTON_RAW_PRESS_QUEUE, &ms, portMAX_DELAY);
  };
}

void button_task(void* parameter) {
  unsigned long ms;
  ButtonPressType event;
  for (;;) {
    if (xQueueReceive(BUTTON_RAW_PRESS_QUEUE, &ms, portMAX_DELAY)) {
      if (xQueueReceive(BUTTON_RAW_PRESS_QUEUE, &ms, DOUBLE_PRESS_THRESHOLD_MS / portTICK_PERIOD_MS)) {
        event = BUTTON_PRESS_DOUBLE;
      } else {
        event = BUTTON_PRESS_SINGLE;
      }
      xQueueSend(BUTTON_PRESS_QUEUE, &event, portMAX_DELAY);
    }
  }
}

void radio_send_task(void* parameter) {
  char* msg;
  int state;
  for (;;) {
    if (xQueueReceive(RADIO_QUEUE, &msg, portMAX_DELAY)) {
      Serial.printf("time from button click to transmission: %lu us\n", micros() - LAST_BUTTON_PRESSED_US);
      Serial.println(msg);
      state = radio.transmit(msg);
      if (state == RADIOLIB_ERR_NONE) {
        Serial.println("transmitted");
      } else {
        Serial.printf("transmission failed, code %d\n", state);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  // init lora -- SX1276 at 868 MHz
  SPI.begin(SPI_LORA_SCK, SPI_LORA_MISO, SPI_LORA_MOSI, SPI_LORA_CS);
  ConfigLoRa_t config;
  config.frequency = 868;
  config.spreadingFactor = LORA_SF;
  config.bandwidth = LORA_BW;
  int state = radio.begin(config);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("failed to init lora, code %d\n", state);
    while (1);
  }

#ifdef IS_SENDER
  BUTTON_RAW_PRESS_QUEUE = xQueueCreate(BUTTON_RAW_PRESS_QUEUE_SIZE, sizeof(unsigned long));
  if (BUTTON_RAW_PRESS_QUEUE == NULL) {
    Serial.println("Failed to create queue (raw button)");
    while (1);
  }

  BUTTON_PRESS_QUEUE = xQueueCreate(BUTTON_PRESS_QUEUE_SIZE, sizeof(ButtonPressType));
  if (BUTTON_PRESS_QUEUE == NULL) {
    Serial.println("Failed to create queue (button)");
    while (1);
  }

  RADIO_QUEUE = xQueueCreate(RADIO_QUEUE_SIZE, sizeof(char*));
  if (RADIO_QUEUE == NULL) {
    Serial.println("Failed to create queue (radio)");
    while (1);
  }

  xTaskCreatePinnedToCore(button_task, "button_task", 4096, NULL, 2, NULL, 1);          // higher priority
  xTaskCreatePinnedToCore(radio_send_task, "radio_send_task", 4096, NULL, 1, NULL, 1);  // lower priority

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_pressed, FALLING);
#endif
}

void loop() {
#ifdef IS_SENDER
  static const char single_msg[] = "single click", double_msg[] = "double click";
  ButtonPressType event;
  if (xQueueReceive(BUTTON_PRESS_QUEUE, &event, portMAX_DELAY)) {
    const char* msg =
        (event == BUTTON_PRESS_SINGLE) ? single_msg : ((event == BUTTON_PRESS_DOUBLE) ? double_msg : NULL);
    if (msg != NULL) {
      xQueueSend(RADIO_QUEUE, &msg, portMAX_DELAY);
    }
  }
#else
  char buf[256];
  int state = radio.receive((uint8_t*)buf, sizeof(buf) - 1, LORA_RECEIVE_TIMEOUT_MS);  // reserve 1 byte for \0
  if (state == RADIOLIB_ERR_NONE) {
    size_t len = radio.getPacketLength();
    buf[len] = '\0';
    Serial.println(buf);
  } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {  // ignore timeouts
    Serial.printf("receiving failed, code %d\n", state);
  }
#endif
}
