#include <Arduino.h>
#include <RadioLib.h>

#define LED_PIN 2
#define LED_BLINKING_INTERVAL_MS 100

#define SPI_LORA_MOSI 27
#define SPI_LORA_MISO 19
#define SPI_LORA_SCK 5
#define SPI_LORA_CS 18
#define SPI_LORA_RST 23
#define SPI_LORA_DIO0 26
#define SPI_LORA_DIO1 33
#define SPI_LORA_DIO2 32

#define LORA_SF 11
#define LORA_BW 125.0
#define LORA_RECEIVE_TIMEOUT_MS 60000                // 1 minute
#define LORA_SEND_MESSAGE_INTERVAL_MS 1000           // each second
#define LORA_SEND_MESSAGE_TIMEOUT_INTERVAL_MS 20000  // 20 seconds

volatile bool transmitted_flag = true;

const uint32_t RADIO_MESSAGE_SIZE = 32;
const char RADIO_MESSAGE[RADIO_MESSAGE_SIZE + 1] = "abcdefghijklmnopqrstuvwxyzABCDEF";  // +1 for \0
static_assert(sizeof(RADIO_MESSAGE) == RADIO_MESSAGE_SIZE + 1);                         // +1 for \0

SPISettings spiSettings(8000000, MSBFIRST, SPI_MODE0);
Module* mod = new Module(SPI_LORA_CS, SPI_LORA_DIO0, SPI_LORA_RST, SPI_LORA_DIO1, SPI, spiSettings);
SX1276 radio = SX1276(mod);

void settransmitted_flag(void) {
  transmitted_flag = true;
}

void led_task(void* parameter) {
  for (;;) {
    digitalWrite(LED_PIN, HIGH);  // turn LED on
    vTaskDelay(LED_BLINKING_INTERVAL_MS / portTICK_PERIOD_MS);
    digitalWrite(LED_PIN, LOW);  // turn LED off
    vTaskDelay(LED_BLINKING_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void radio_send_task(void* parameter) {
  int16_t start_transmission_state = RADIOLIB_ERR_NONE, finish_transmission_state = RADIOLIB_ERR_NONE;
  unsigned long transmission_started_ms = 0;
  radio.finishTransmit();
  for (;;) {
    if (transmitted_flag) {               // start a new transmission
      if (transmission_started_ms > 0) {  // handle previous transmission results
        finish_transmission_state = radio.finishTransmit();
        if (start_transmission_state == RADIOLIB_ERR_NONE && finish_transmission_state == RADIOLIB_ERR_NONE) {
          Serial.printf("transmitted, transmission took ms: %lu\n", millis() - transmission_started_ms);
        } else {
          Serial.printf("transmission failed, start code %d, finish code %d\n", start_transmission_state,
                        finish_transmission_state);
        }
        vTaskDelay(LORA_SEND_MESSAGE_INTERVAL_MS / portTICK_PERIOD_MS);
      }
      transmitted_flag = false;
      start_transmission_state = radio.startTransmit(RADIO_MESSAGE);
      transmission_started_ms = millis();
    } else {
      if (millis() - transmission_started_ms > LORA_SEND_MESSAGE_TIMEOUT_INTERVAL_MS) {  // just in case
        Serial.printf("transmission timeout - transmission failed for some reason; resetting transmission");
        transmitted_flag = true;
      }
      vTaskDelay(20 / portTICK_PERIOD_MS);  // wait 20 ms
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
  // init and start LED blinking
  pinMode(LED_PIN, OUTPUT);
  xTaskCreatePinnedToCore(led_task, "led_task", 4096, NULL, 1, NULL, 1);

  // init radio transmission
  radio.setPacketSentAction(settransmitted_flag);
  xTaskCreatePinnedToCore(radio_send_task, "radio_send_task", 4096, NULL, 2, NULL, 1);  // same core & higher priority
#endif
}

void loop() {
#ifndef IS_SENDER
  // receiver code
  char buf[RADIO_MESSAGE_SIZE + 1];
  int state = radio.receive((uint8_t*)buf, sizeof(buf) - 1, LORA_RECEIVE_TIMEOUT_MS);  // reserve 1 byte for \0
  if (state == RADIOLIB_ERR_NONE) {
    size_t len = radio.getPacketLength();
    buf[len] = '\0';
    Serial.println("received:");
    Serial.println(buf);
  } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {  // ignore timeouts
    Serial.printf("receiving failed, code %d\n", state);
  }
#endif
}
