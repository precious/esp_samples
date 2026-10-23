#include <Arduino.h>
#include <LittleFS.h>
#include <mavlink.h>
#include <mavlink_telemetry_csv.h>

void setup() {
  Serial.begin(115200);
  Serial2.begin(57600, SERIAL_8N1, 4, 25);  // RX = GPIO4, TX = GPIO25
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }
  Serial.printf("Total: %u bytes\n", LittleFS.totalBytes());
  Serial.printf("Used:  %u bytes\n", LittleFS.usedBytes());
  Serial.printf("Free:  %u bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
}

void loop() {
  const int max_fields = 16;
  static MessageField fields[max_fields];
  while (Serial2.available() > 0) {
    const uint8_t c = Serial2.read();
    mavlink_message_t msg;
    mavlink_status_t status;
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      if (formatter_map.find(msg.msgid) != formatter_map.end()) {
        const auto& formatter = formatter_map[msg.msgid];
        auto message_meta = formatter->get_message_meta();
        Serial.printf("[%s] ", message_meta.message_type_name.c_str());
        ;
        for (const auto& h : message_meta.header) {
          Serial.printf("%s ", h.c_str());
        }
        int num_fields = formatter->extract_message_fields(msg, fields, max_fields);
        if (num_fields != message_meta.header.size()) {
          Serial.printf("Warning: number of extracted fields (%d) does not match header size (%d)\n", num_fields,
                        static_cast<int>(message_meta.header.size()));
        } else {
          for (int i = 0; i < num_fields; ++i) {
            switch (fields[i].type) {
              case FieldDataType::int64_t:
                Serial.print(fields[i].int64_t_value);
                break;
              case FieldDataType::uint64_t:
                Serial.print(fields[i].uint64_t_value);
                break;
              case FieldDataType::float_:
                Serial.print(fields[i].float_value);
                break;
              case FieldDataType::string_:
                Serial.print(fields[i].string_value);
                break;
              default:
                Serial.print("UNKNOWN");
                break;
            }
            Serial.print(" ");
          }
          Serial.println();
        }
      }
    }
  }
}
