const int PWM_PIN = 15;

volatile uint32_t pulseStart = 0;
volatile uint32_t pulseWidth = 0;
volatile bool pulseReady = false;

void IRAM_ATTR onPwmChange() {
  uint32_t now = micros();

  if (digitalRead(PWM_PIN)) {
    pulseStart = now;  // Rising edge
  } else {
    pulseWidth = now - pulseStart;  // Falling edge
    pulseReady = true;
  }
}

void onCommand2000us() {
  Serial.println("Received 2000 us command!");
}

void setup() {
  Serial.begin(115200);
  pinMode(PWM_PIN, INPUT);

  attachInterrupt(
    digitalPinToInterrupt(PWM_PIN),
    onPwmChange,
    CHANGE
  );
}

void loop() {
  if (pulseReady) {
    noInterrupts();
    uint32_t width = pulseWidth;
    pulseReady = false;
    interrupts();

    if (width >= 1950 && width <= 2050) {
      onCommand2000us();
    }
  }

  // Other work proceeds without waiting for a pulse.
}