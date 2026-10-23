#include <Arduino.h>
#include <LittleFS.h>
#include <mavlink.h>
#include <mavlink_telemetry_formatters.h>

#include <queue>
#include <unordered_map>

#define FLUSH_INTERVAL_MS 100
#define MAVLINK_RX_PIN 4
#define MAVLINK_TX_PIN 25

void setup() {
  Serial.begin(115200);
  Serial2.begin(57600, SERIAL_8N1, MAVLINK_RX_PIN, MAVLINK_TX_PIN);
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }
  Serial.printf("Total: %u bytes\n", LittleFS.totalBytes());
  Serial.printf("Used:  %u bytes\n", LittleFS.usedBytes());
  Serial.printf("Free:  %u bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
}

class StreamingMAVLinkTelemetryCSVWriter {
  std::unordered_map<int, std::queue<mavlink_message_t>> _queue_by_msg_id;
  std::vector<MAVLinkMessageFormatterBase*> _formatters;
  std::unordered_map<int, MAVLinkMessageFormatterBase*> _formatters_map;
  inline static int MAX_QUEUE_SIZE = 100;  // per message ID
  inline const static int MAX_FIELDS_NUM = 16;
  MessageField fields[MAX_FIELDS_NUM];

  template <typename T>
  static void _print_to_streams(std::vector<Stream*>& streams, T val) {
    for (auto& stream : streams) {
      stream->print(val);
    }
  }

 public:
  StreamingMAVLinkTelemetryCSVWriter(std::vector<MAVLinkMessageFormatterBase*> formatters,
                                     std::unordered_map<int, MAVLinkMessageFormatterBase*> formatters_map) {
    _formatters = formatters;
    _formatters_map = formatters_map;
    for (const auto& kv : _formatters_map) {
      _queue_by_msg_id[kv.first] = std::queue<mavlink_message_t>();
    }
  }

  void queue(mavlink_message_t msg) {
    auto it = _queue_by_msg_id.find(msg.msgid);
    if (it == _queue_by_msg_id.end()) {
      return;
    }
    auto& msg_queue = it->second;
    if (msg_queue.size() < MAX_QUEUE_SIZE + 1) {  // allow +1 to detect overflow
      msg_queue.push(msg);
    }
  }

  void write_header(std::vector<Stream*>& streams) {
    bool first = true;
    for (const auto& formatter : _formatters) {
      auto message_meta = formatter->get_message_meta();
      for (int j = 0; j < message_meta.header.size(); ++j) {
        if (!first) {
          _print_to_streams(streams, ",");
        }
        first = false;
        _print_to_streams(streams, message_meta.header[j].c_str());
      }
    }
    _print_to_streams(streams, "\n");
  }

  void flush(std::vector<Stream*>& streams) {
    // check queues first
    int max_queue_size = 0;
    bool overflow = false;
    for (auto& kv : _queue_by_msg_id) {
      while (kv.second.size() > MAX_QUEUE_SIZE) {  // overflow, drop extra
        overflow = true;
        kv.second.pop();
      }
      max_queue_size = std::max(max_queue_size, (int)kv.second.size());
    }

    // write values
    for (int line = 0; line < max_queue_size; ++line) {
      bool first = true;
      for (const auto& formatter : _formatters) {
        auto& msg_queue = _queue_by_msg_id[formatter->get_message_meta().mavlink_msg_id];
        int num_header_fields = formatter->get_message_meta().header.size();
        if (!msg_queue.empty()) {
          mavlink_message_t msg = msg_queue.front();
          msg_queue.pop();
          int num_fields = formatter->extract_message_fields(&msg, fields, MAX_FIELDS_NUM);
          if (num_fields != num_header_fields) {
            _print_to_streams(streams, "ERROR: FIELDS NUM MISMATCH");
          }
          for (int i = 0; i < num_fields; ++i) {
            if (!first) {
              _print_to_streams(streams, ",");
            }
            first = false;
            switch (fields[i].type) {
              case FieldDataType::int64_t:
                _print_to_streams(streams, fields[i].int64_t_value);
                break;
              case FieldDataType::uint64_t:
                _print_to_streams(streams, fields[i].uint64_t_value);
                break;
              case FieldDataType::float_:
                _print_to_streams(streams, fields[i].float_value);
                break;
              case FieldDataType::string_:
                _print_to_streams(streams, "\"");
                _print_to_streams(streams, fields[i].string_value);
                _print_to_streams(streams, "\"");
                break;
              default:
                _print_to_streams(streams, "UNKNOWN");
                break;
            }
          }
        } else {  // print just commas for missing fields
          for (int i = 0; i < num_header_fields; ++i) {
            if (!first) {
              _print_to_streams(streams, ",");
            }
            first = false;
          }
        }
      }
      _print_to_streams(streams, "\n");
    }

    // error message if needed
    if (overflow) {
      _print_to_streams(streams, "ERROR: QUEUE OVERFLOW, SOME MESSAGES MAY BE DROPPED\n");
    }
  }
};

void loop() {
  // static const int max_fields = 16;
  // static MessageField fields[max_fields];
  static bool first_run = true;
  static unsigned long last_flush_time = 0;
  static std::vector<Stream*> streams = {&Serial};
  static StreamingMAVLinkTelemetryCSVWriter csv_writer(formatters, formatters_map);
  if (first_run) {
    first_run = false;
    csv_writer.write_header(streams);
  }

  while (Serial2.available() > 0) {
    const uint8_t c = Serial2.read();
    mavlink_message_t msg;
    mavlink_status_t status;
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      csv_writer.queue(msg);
    }
  }

  if (millis() - last_flush_time >= FLUSH_INTERVAL_MS) {
    csv_writer.flush(streams);
    last_flush_time = millis();
  }
}
