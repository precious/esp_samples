#include <Arduino.h>

void setup() { Serial.begin(115200); }

void func2() {
  // should crash
  char str[32];
  memset(str, 65, sizeof(str) + 10);  // here
  Serial.print("str: ");
  Serial.println(str);  // or here
}

// this one causes "Stack smashing protect failure!" crash
void func1() { func2(); }

void func4() {
  volatile int* p = NULL;
  *p = 65;
}

// this one causes "Guru Meditation Error: Core  1 panic'ed (StoreProhibited). Exception was unhandled." crash
void func3() { func4(); }

void loop() {
  delay(2000);
  Serial.println("start");
  // func1();  // "Stack smashing protect failure!" crash
  func4();  // "Guru Meditation Error: Core  1 panic'ed (StoreProhibited). Exception was unhandled." crash
}

/*
start
str: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAx

Stack smashing protect failure!


Backtrace: 0x4008344d:0x3ffb21c0 0x400880e1:0x3ffb21e0 0x4008284a:0x3ffb2200 0x400d1421:0x3ffb2220 0x400d1434:0x3ffb2270
0x400d30cd:0x3ffb2290 #0  0x4008344d in panic_abort at
/home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/esp_system/panic.c:408 #1
0x400880e1 in esp_system_abort at
/home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/esp_system/esp_system.c:137 #2
0x4008284a in __stack_chk_fail at
/home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/esp_system/stack_check.c:35 #3
0x400d1421 in func2() at src/main.cpp:11 #4  0x400d1434 in func1() at src/main.cpp:13 (inlined by) loop() at
src/main.cpp:17 #5  0x400d30cd in loopTask(void*) at
/Users/vsevolodkulaga/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:50





ELF file SHA256: b9c436573ea03736

Rebooting...
ets Jul 29 2019 12:21:46

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
  #0  0x40080400 in _init at ??:?

entry 0x400805e4
*/

// -----------------------------------------------------------------

/*
Guru Meditation Error: Core  1 panic'ed (StoreProhibited). Exception was unhandled.

Core  1 register dump:
PC      : 0x400d13fc  PS      : 0x00060830  A0      : 0x800d3084  A1      : 0x3ffb2270
  #0  0x400d13fc in func4() at src/main.cpp:17
      (inlined by) loop() at src/main.cpp:25

A2      : 0x00000000  A3      : 0x00000000  A4      : 0x00000014  A5      : 0x00000004
A6      : 0x3ffb8188  A7      : 0x80000001  A8      : 0x00000000  A9      : 0x3ffb2250
A10     : 0x00000007  A11     : 0x00000005  A12     : 0x00000078  A13     : 0x0000e100
A14     : 0x00000001  A15     : 0x3ffbdb68  SAR     : 0x0000001d  EXCCAUSE: 0x0000001d
EXCVADDR: 0x00000000  LBEG    : 0x40086431  LEND    : 0x40086441  LCOUNT  : 0xffffffff


Backtrace: 0x400d13f9:0x3ffb2270 0x400d3081:0x3ffb2290
  #0  0x400d13f9 in func4() at src/main.cpp:17
      (inlined by) loop() at src/main.cpp:25
  #1  0x400d3081 in loopTask(void*) at /Users/vsevolodkulaga/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:50





ELF file SHA256: 0cb5638577388a91

Rebooting...
ets Jul 29 2019 12:21:46

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
  #0  0x40080400 in _init at ??:?

entry 0x400805e4
*/