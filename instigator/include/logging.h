// #include <Arduino.h>
// #include <LittleFS.h>

// constexpr char LOG_FILE[] = "/log.txt";
// constexpr char OLD_FILE[] = "/log.1";
// constexpr size_t MAX_LOG_BYTES = 64 * 1024;

// bool initLogger() {
//     // Do not automatically format the filesystem on mount failure.
//     if (!LittleFS.begin(false)) {
//         Serial.println("LittleFS mount failed");
//         return false;
//     }

//     Serial.printf("Log storage: %u / %u bytes used\n",
//                   (unsigned)LittleFS.usedBytes(),
//                   (unsigned)LittleFS.totalBytes());
//     return true;
// }

// void logLine(char level, const char* tag, const char* message) {
//     char line[128];

//     // Uptime in seconds, not calendar time.
//     int n = snprintf(line, sizeof(line), "%lu %c %s %s\n",
//                      (unsigned long)(millis() / 1000),
//                      level, tag, message);

//     if (n <= 0) return;

//     size_t len = (n >= (int)sizeof(line))
//                    ? sizeof(line) - 1
//                    : (size_t)n;

//     // Ensure truncated messages still end with a newline.
//     if (n >= (int)sizeof(line)) {
//         line[len - 1] = '\n';
//     }

//     File f = LittleFS.open(LOG_FILE, FILE_APPEND);
//     if (!f) return;

//     if (f.size() + len > MAX_LOG_BYTES) {
//         f.close();

//         LittleFS.remove(OLD_FILE);

//         if (LittleFS.exists(LOG_FILE) &&
//             !LittleFS.rename(LOG_FILE, OLD_FILE)) {
//             return; // Avoid unbounded growth if rotation fails.
//         }

//         f = LittleFS.open(LOG_FILE, FILE_APPEND);
//         if (!f) return;
//     }

//     f.write((const uint8_t*)line, len);
//     f.close();
// }

// void setup() {
//     Serial.begin(115200);

//     if (!initLogger()) return;

//     logLine('I', "SYS", "boot");
//     logLine('W', "GPS", "no fix");
//     logLine('E', "SENS", "read failed");
// }

// void loop() {
// }