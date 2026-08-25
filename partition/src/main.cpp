#include <Arduino.h>
#include <esp_partition.h>

#include <iomanip>
#include <iostream>
#include <sstream>

void setup() {
  Serial.begin(115200);
}

struct SizeFormatted {
  unsigned int num;
  char suffix;  // 0 if no suffix

  SizeFormatted() : num(0), suffix(0) {};
  SizeFormatted(int num, char suffix) : num(num), suffix(suffix) {};

  static SizeFormatted from_bytes(unsigned int bytes) {
    if (bytes >= 1024 * 1024 && bytes == (bytes / (1024 * 1024)) * (1024 * 1024)) {
      return SizeFormatted(bytes / (1024 * 1024), 'M');
    } else if (bytes >= 1024 && bytes == (bytes / 1024) * 1024) {
      return SizeFormatted(bytes / 1024, 'K');
    } else {
      return SizeFormatted(bytes, 0);
    }
  }
};

void print_partition_csv_header(std::ostream& stream) {
  stream << "Name,Type,SubType,Offset,Size,Flags\n";
}

void print_partition_csv_row(const esp_partition_t* partition, std::ostream& stream) {
  SizeFormatted size_frmtd;
  // name
  stream << partition->label << ',';
  // type, subtype
  if (partition->type == ESP_PARTITION_TYPE_APP) {
    stream << "app,";
    if (partition->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY)
      stream << "factory";
    else if (partition->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
             partition->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX)
      stream << "ota_" << (partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
    else
      stream << "unknown";  // should not happen
  } else if (partition->type == ESP_PARTITION_TYPE_DATA) {
    stream << "data,";
    if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_OTA)
      stream << "ota";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_PHY)
      stream << "phy";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
      stream << "nvs";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_COREDUMP)
      stream << "coredump";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS)
      stream << "nvs_keys";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM)
      stream << "efuse_em";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_UNDEFINED)
      stream << "undefined";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD)
      stream << "esphttpd";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_FAT)
      stream << "fat";
    else if (partition->subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS)
      stream << "spiffs";
    else
      stream << "unknown";  // should not happen
  } else
    stream << "unknown,unknown";  // should not happen
  stream << ",";
  // offset
  size_frmtd = SizeFormatted::from_bytes(partition->address);
  stream << size_frmtd.num;
  if (size_frmtd.suffix != 0) stream << size_frmtd.suffix;
  stream << ",";
  // size
  size_frmtd = SizeFormatted::from_bytes(partition->size);
  stream << size_frmtd.num;
  if (size_frmtd.suffix != 0) stream << size_frmtd.suffix;
  stream << ",";
  // flags
  if (partition->encrypted) stream << "encrypted";
  stream << '\n';
}

void loop() {
  // https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/partition_table.html
  // https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-reference/storage/partition.html

  // 1. print all partitions in CSV format
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (it == NULL) {
    Serial.println("No partitions found");
  } else {  // print CSV
    std::ostringstream oss;
    print_partition_csv_header(oss);
    while (it != NULL) {
      print_partition_csv_row(esp_partition_get(it), oss);
      it = esp_partition_next(it);
    }

    esp_partition_iterator_release(it);

    Serial.print(oss.str().c_str());
  }

  // 2. read and print binary data from the "data -> ota" parition (the smallest one - 8K)
  const esp_partition_t* parition =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  if (parition == NULL) {
    Serial.println("No data -> ota partition found");
  } else {
    Serial.println("data -> ota partition binary (hex) content:");
    const int BUF_SIZE = 512;
    u_int8_t buf[BUF_SIZE];
    esp_err_t error;
    int chunk_size;
    std::ostringstream oss;
    for (int src_offset = 0; src_offset < parition->size; src_offset += BUF_SIZE) {
      chunk_size = min(BUF_SIZE, int(parition->size - src_offset));
      error = esp_partition_read(parition, src_offset, buf, chunk_size);
      if (error != ESP_OK) {
        Serial.printf("Error code returned by esp_partition_read(): %d\n", error != ESP_OK);
        break;
      }
      for (int i = 0; i < chunk_size; i++)
        oss << ((i == 0) ? "" : " ") << std::hex << std::setw(2) << std::setfill('0') << std::uppercase
            << static_cast<int>(buf[i]);
      Serial.print(oss.str().c_str());
      oss.str("");
    }
    Serial.print('\n');
  }

  delay(5000);
}
