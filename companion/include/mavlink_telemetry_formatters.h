#include <mavlink.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class FieldDataType { int64_t, uint64_t, float_, string_ };

struct MessageField {
  FieldDataType type;
  int64_t int64_t_value;
  uint64_t uint64_t_value;
  float float_value;
  char string_value[32];

  MessageField() : type(FieldDataType::int64_t), int64_t_value(0) {}
  MessageField(int8_t i) : type(FieldDataType::int64_t), int64_t_value(i) {}
  MessageField(uint8_t i) : type(FieldDataType::uint64_t), uint64_t_value(i) {}
  MessageField(int16_t i) : type(FieldDataType::int64_t), int64_t_value(i) {}
  MessageField(uint16_t i) : type(FieldDataType::uint64_t), uint64_t_value(i) {}
  MessageField(int32_t i) : type(FieldDataType::int64_t), int64_t_value(i) {}
  MessageField(uint32_t i) : type(FieldDataType::uint64_t), uint64_t_value(i) {}
  MessageField(int64_t i) : type(FieldDataType::int64_t), int64_t_value(i) {}
  MessageField(uint64_t i) : type(FieldDataType::uint64_t), uint64_t_value(i) {}
  MessageField(float f) : type(FieldDataType::float_), float_value(f) {}
  MessageField(FieldDataType t) : type(t) {}
  MessageField(char* s) : type(FieldDataType::string_) {
    snprintf(string_value, sizeof(string_value), "%s", s);
  }
  MessageField(std::string s) : type(FieldDataType::string_) {
    snprintf(string_value, sizeof(string_value), "%s", s.c_str());
  }
};

struct MessageMeta {
  int mavlink_msg_id;
  std::string message_type_name;
  std::vector<std::string> header;
};

class MAVLinkMessageFormatterBase {
  // https://mavlink.io/en/messages/common.html
  // https://mavlink.io/en/messages/ardupilotmega.html
 public:
  virtual ~MAVLinkMessageFormatterBase() = default;
  virtual MessageMeta get_message_meta() = 0;
  // extract message fields into the provided array of pairs (type, pointer to value). return the number of fields
  // extracted. if number of fields exceeds the provided array size, some fields are silently dropped
  virtual int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) = 0;
};

class MAVLinkSysStatusMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{
        MAVLINK_MSG_ID_SYS_STATUS, "SYS_STATUS", {"battery (mV)", "current (cA)", "remaining (%)", "load (0-1000)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_sys_status_t p;
    mavlink_msg_sys_status_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.voltage_battery);
    fields[std::min(counter++, size - 1)] = MessageField(p.current_battery);
    fields[std::min(counter++, size - 1)] = MessageField(p.battery_remaining);
    fields[std::min(counter++, size - 1)] = MessageField(p.load);
    return std::min(counter, size);
  }
};

class MAVLinkPowerStatusMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_POWER_STATUS, "POWER_STATUS", {"vcc (mV)", "servo (mV)", "flags (hex)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_power_status_t p;
    mavlink_msg_power_status_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.Vcc);
    fields[std::min(counter++, size - 1)] = MessageField(p.Vservo);
    int index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "0x%04X", p.flags);
    return std::min(counter, size);
  }
};

class MAVLinkMeminfoMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_MEMINFO, "MEMINFO", {"heap_top (bytes)", "free (bytes)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_meminfo_t p;
    mavlink_msg_meminfo_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.brkval);
    fields[std::min(counter++, size - 1)] = MessageField(p.freemem32 ? p.freemem32 : p.freemem);
    return std::min(counter, size);
  }
};

class MAVLinkNavControllerOutputMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT,
                       "NAV_CONTROLLER_OUTPUT",
                       {"target_roll (deg)", "target_pitch (deg)", "bearing (deg)", "target_bearing (deg)",
                        "wp_distance (m)", "alt_error (m)", "xtrack_error (m)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_nav_controller_output_t p;
    mavlink_msg_nav_controller_output_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.nav_roll);
    fields[std::min(counter++, size - 1)] = MessageField(p.nav_pitch);
    fields[std::min(counter++, size - 1)] = MessageField(p.nav_bearing);
    fields[std::min(counter++, size - 1)] = MessageField(p.target_bearing);
    fields[std::min(counter++, size - 1)] = MessageField(p.wp_dist);
    fields[std::min(counter++, size - 1)] = MessageField(p.alt_error);
    fields[std::min(counter++, size - 1)] = MessageField(p.xtrack_error);
    return std::min(counter, size);
  }
};

class MAVLinkMissionCurrentMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_MISSION_CURRENT, "MISSION_CURRENT", {"sequence"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_mission_current_t p;
    mavlink_msg_mission_current_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.seq);
    return std::min(counter, size);
  }
};

class MAVLinkServoOutputRawMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_SERVO_OUTPUT_RAW, "SERVO_OUTPUT_RAW", {"port", "servo1_to_8"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_servo_output_raw_t p;
    mavlink_msg_servo_output_raw_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.port);
    int index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%u,%u,%u,%u,%u,%u,%u,%u", p.servo1_raw,
             p.servo2_raw, p.servo3_raw, p.servo4_raw, p.servo5_raw, p.servo6_raw, p.servo7_raw, p.servo8_raw);
    return std::min(counter, size);
  }
};

class MAVLinkRcChannelsMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_RC_CHANNELS, "RC_CHANNELS", {"count", "rssi", "ch1_to_8"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_rc_channels_t p;
    mavlink_msg_rc_channels_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.chancount);
    fields[std::min(counter++, size - 1)] = MessageField(p.rssi);
    const int index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%u,%u,%u,%u,%u,%u,%u,%u", p.chan1_raw,
             p.chan2_raw, p.chan3_raw, p.chan4_raw, p.chan5_raw, p.chan6_raw, p.chan7_raw, p.chan8_raw);
    return std::min(counter, size);
  }
};

class MAVLinkRawImuMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_RAW_IMU, "RAW_IMU", {"accel", "gyro", "mag"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_raw_imu_t p;
    mavlink_msg_raw_imu_decode(msg, &p);
    const std::array<std::array<int16_t, 3>, 3> vectors = {
        {{p.xacc, p.yacc, p.zacc}, {p.xgyro, p.ygyro, p.zgyro}, {p.xmag, p.ymag, p.zmag}}};
    for (const auto& vector : vectors) {
      const int index = std::min(counter++, size - 1);
      fields[index] = MessageField(FieldDataType::string_);
      snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%d,%d,%d", vector[0], vector[1],
               vector[2]);
    }
    return std::min(counter, size);
  }
};

class MAVLinkScaledPressureMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_SCALED_PRESSURE,
                       "SCALED_PRESSURE",
                       {"absolute (hPa)", "differential (hPa)", "temperature (C)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_scaled_pressure_t p;
    mavlink_msg_scaled_pressure_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.press_abs);
    fields[std::min(counter++, size - 1)] = MessageField(p.press_diff);
    fields[std::min(counter++, size - 1)] = MessageField(p.temperature / 100.0f);
    return std::min(counter, size);
  }
};

class MAVLinkGpsRawIntMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{
        MAVLINK_MSG_ID_GPS_RAW_INT,
        "GPS_RAW_INT",
        {"fix", "sats", "lat_lon", "alt (m)", "eph (m)", "epv (m)", "vel (m/s)", "cog (deg)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_gps_raw_int_t p;
    mavlink_msg_gps_raw_int_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.fix_type);
    fields[std::min(counter++, size - 1)] = MessageField(p.satellites_visible);
    int index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%.7f,%.7f", p.lat / 1e7f,
             p.lon / 1e7f);
    fields[std::min(counter++, size - 1)] = MessageField(p.alt / 1000.0f);
    fields[std::min(counter++, size - 1)] = MessageField(p.eph / 100.0f);
    fields[std::min(counter++, size - 1)] = MessageField(p.epv / 100.0f);
    fields[std::min(counter++, size - 1)] = MessageField(p.vel / 100.0f);
    fields[std::min(counter++, size - 1)] = MessageField(p.cog / 100.0f);
    return std::min(counter, size);
  }
};

class MAVLinkSystemTimeMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_SYSTEM_TIME, "SYSTEM_TIME", {"unix_us", "boot_ms"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_system_time_t p;
    mavlink_msg_system_time_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.time_unix_usec);
    fields[std::min(counter++, size - 1)] = MessageField(p.time_boot_ms);
    return std::min(counter, size);
  }
};

class MAVLinkTerrainReportMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_TERRAIN_REPORT,
                       "TERRAIN_REPORT",
                       {"lat_lon", "spacing (m)", "terrain (m)", "current_height (m)", "pending", "loaded"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_terrain_report_t p;
    mavlink_msg_terrain_report_decode(msg, &p);
    int index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%.7f,%.7f", p.lat / 1e7f,
             p.lon / 1e7f);
    fields[std::min(counter++, size - 1)] = MessageField(p.spacing);
    fields[std::min(counter++, size - 1)] = MessageField(p.terrain_height);
    fields[std::min(counter++, size - 1)] = MessageField(p.current_height);
    fields[std::min(counter++, size - 1)] = MessageField(p.pending);
    fields[std::min(counter++, size - 1)] = MessageField(p.loaded);
    return std::min(counter, size);
  }
};

class MAVLinkEkfStatusReportMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_EKF_STATUS_REPORT,
                       "EKF_STATUS_REPORT",
                       {"flags (hex)", "velocity_var", "pos_horiz_var", "pos_vert_var", "compass_var",
                        "terrain_var"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_ekf_status_report_t p;
    mavlink_msg_ekf_status_report_decode(msg, &p);
    int index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "0x%04X", p.flags);
    fields[std::min(counter++, size - 1)] = MessageField(p.velocity_variance);
    fields[std::min(counter++, size - 1)] = MessageField(p.pos_horiz_variance);
    fields[std::min(counter++, size - 1)] = MessageField(p.pos_vert_variance);
    fields[std::min(counter++, size - 1)] = MessageField(p.compass_variance);
    fields[std::min(counter++, size - 1)] = MessageField(p.terrain_alt_variance);
    return std::min(counter, size);
  }
};

class MAVLinkVibrationMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_VIBRATION, "VIBRATION", {"vibration", "clipping"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_vibration_t p;
    mavlink_msg_vibration_decode(msg, &p);
    int index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%.9g,%.9g,%.9g", p.vibration_x,
             p.vibration_y, p.vibration_z);
    index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%lu,%lu,%lu",
             static_cast<unsigned long>(p.clipping_0), static_cast<unsigned long>(p.clipping_1),
             static_cast<unsigned long>(p.clipping_2));
    return std::min(counter, size);
  }
};

class MAVLinkAttitudeMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{
        MAVLINK_MSG_ID_ATTITUDE,
        "ATTITUDE",
        {"roll (deg)", "pitch (deg)", "yaw (deg)", "rollspeed (deg/s)", "pitchspeed (deg/s)",
         "yawspeed (deg/s)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_attitude_t p;
    mavlink_msg_attitude_decode(msg, &p);
    constexpr float radians_to_degrees = 57.2957795f;
    fields[std::min(counter++, size - 1)] = MessageField(p.roll * radians_to_degrees);
    fields[std::min(counter++, size - 1)] = MessageField(p.pitch * radians_to_degrees);
    fields[std::min(counter++, size - 1)] = MessageField(p.yaw * radians_to_degrees);
    fields[std::min(counter++, size - 1)] = MessageField(p.rollspeed * radians_to_degrees);
    fields[std::min(counter++, size - 1)] = MessageField(p.pitchspeed * radians_to_degrees);
    fields[std::min(counter++, size - 1)] = MessageField(p.yawspeed * radians_to_degrees);
    return std::min(counter, size);
  }
};

class MAVLinkGlobalPositionIntMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
                       "GLOBAL_POSITION_INT",
                       {"lat_lon", "alt_msl (m)", "relative_alt (m)", "velocity_ned (m/s)", "heading (deg)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_global_position_int_t p;
    mavlink_msg_global_position_int_decode(msg, &p);
    int index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%.7f,%.7f", p.lat / 1e7f, p.lon / 1e7f);
    fields[std::min(counter++, size - 1)] = MessageField(p.alt / 1000.0f);
    fields[std::min(counter++, size - 1)] = MessageField(p.relative_alt / 1000.0f);
    index = std::min(counter++, size - 1);
    fields[index] = MessageField(FieldDataType::string_);
    snprintf(fields[index].string_value, sizeof(fields[index].string_value), "%.2f,%.2f,%.2f", p.vx / 100.0f, p.vy / 100.0f, p.vz / 100.0f);
    fields[std::min(counter++, size - 1)] = MessageField(p.hdg / 100.0f);
    return std::min(counter, size);
  }
};

class MAVLinkVfrHudMessageFormatter : public MAVLinkMessageFormatterBase {
 public:
  MessageMeta get_message_meta() override {
    return MessageMeta{
        MAVLINK_MSG_ID_VFR_HUD,
        "VFR_HUD",
        {"airspeed (m/s)", "groundspeed (m/s)", "heading (deg)", "throttle (%)", "alt (m)", "climb (m/s)"}};
  }

  int extract_message_fields(mavlink_message_t* msg, MessageField* fields, int size) override {
    int counter = 0;
    mavlink_vfr_hud_t p;
    mavlink_msg_vfr_hud_decode(msg, &p);
    fields[std::min(counter++, size - 1)] = MessageField(p.airspeed);
    fields[std::min(counter++, size - 1)] = MessageField(p.groundspeed);
    fields[std::min(counter++, size - 1)] = MessageField(p.heading);
    fields[std::min(counter++, size - 1)] = MessageField(p.throttle);
    fields[std::min(counter++, size - 1)] = MessageField(p.alt);
    fields[std::min(counter++, size - 1)] = MessageField(p.climb);
    return std::min(counter, size);
  }
};

std::vector<MAVLinkMessageFormatterBase*> formatters = {
    new MAVLinkSysStatusMessageFormatter(),       new MAVLinkPowerStatusMessageFormatter(),
    new MAVLinkMeminfoMessageFormatter(),         new MAVLinkNavControllerOutputMessageFormatter(),
    new MAVLinkMissionCurrentMessageFormatter(),  new MAVLinkServoOutputRawMessageFormatter(),
    new MAVLinkRcChannelsMessageFormatter(),      new MAVLinkRawImuMessageFormatter(),
    new MAVLinkScaledPressureMessageFormatter(),  new MAVLinkGpsRawIntMessageFormatter(),
    new MAVLinkSystemTimeMessageFormatter(),      new MAVLinkTerrainReportMessageFormatter(),
    new MAVLinkEkfStatusReportMessageFormatter(), new MAVLinkVibrationMessageFormatter(),
    new MAVLinkAttitudeMessageFormatter(),        new MAVLinkGlobalPositionIntMessageFormatter(),
    new MAVLinkVfrHudMessageFormatter()};

std::unordered_map<int, MAVLinkMessageFormatterBase*> _make_map_from_array(
    const std::vector<MAVLinkMessageFormatterBase*>& formatters) {
  std::unordered_map<int, MAVLinkMessageFormatterBase*> map;
  for (const auto& formatter : formatters) {
    map[formatter->get_message_meta().mavlink_msg_id] = formatter;
  }
  return map;
}
std::unordered_map<int, MAVLinkMessageFormatterBase*> formatters_map = _make_map_from_array(formatters);
