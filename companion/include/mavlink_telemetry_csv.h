#include <Arduino.h>
#include <mavlink.h>
#include <mavlink_telemetry_formatters.h>

#include <queue>
#include <unordered_map>

#define FLUSH_INTERVAL_MS 100

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
    _print_to_streams(streams, "companion_uptime_ms");
    for (const auto& formatter : _formatters) {
      auto message_meta = formatter->get_message_meta();
      for (int j = 0; j < message_meta.header.size(); ++j) {
        _print_to_streams(streams, ",");
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
      _print_to_streams(streams, millis());
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
            _print_to_streams(streams, ",");
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
            _print_to_streams(streams, ",");
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