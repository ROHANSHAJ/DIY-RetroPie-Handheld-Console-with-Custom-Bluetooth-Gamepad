#include <Arduino.h>
#include <BleGamepad.h>

// === BUTTON MAPPINGS ===
#define BUTTON_A 23
#define BUTTON_B 22
#define BUTTON_C 21
#define BUTTON_D 19
#define BUTTON_E 18
#define BUTTON_F 5
#define BUTTON_K 25
#define BUTTON_G 26
#define BUTTON_H 27
#define BUTTON_I 32
#define BUTTON_J 33
#define BUTTON_L 12
#define BUTTON_M 14
#define BUTTON_N 13
#define BUTTON_O 16  // l1
#define BUTTON_P 17  // r1
#define BT_LED 2

#define NUM_BUTTONS 16
int buttonPins[NUM_BUTTONS] = {
  BUTTON_A, BUTTON_B, BUTTON_C, BUTTON_D,
  BUTTON_E, BUTTON_F, BUTTON_K, BUTTON_G,
  BUTTON_H, BUTTON_I, BUTTON_J, BUTTON_L,
  BUTTON_M, BUTTON_N, BUTTON_O, BUTTON_P
};

int gamepadButtons[NUM_BUTTONS] = {
  1, 2, 3, 4, 5, 6, 14, 7,
  8, 9, 10, 11, 12, 13, 15, 16
};

// === JOYSTICK PINS ===
#define JOY1_X 15
#define JOY1_Y 4
#define JOY2_X 34
#define JOY2_Y 35

// === CALIBRATION VALUES ===
#define DEAD_ZONE 300
#define SMOOTHING_FACTOR 0.2
#define JOY_MIN 0
#define JOY_MAX 4095
#define JOY1_X_CENTER 1933
#define JOY1_Y_CENTER 1858
#define JOY2_X_CENTER 2000
#define JOY2_Y_CENTER 2000

bool lastButtonStates[NUM_BUTTONS] = {0};

uint16_t prevLX = 16384, prevLY = 16384;
uint16_t prevRX = 16384, prevRY = 16384;

BleGamepad bleGamepad("DualStick Gamepad", "ESP32", 100);
BleGamepadConfiguration bleGamepadConfig;

uint16_t calibrateAxis(int val, int centerVal, bool invert = false) {
  int offset = abs(val - centerVal);
  if (offset < DEAD_ZONE) return 16384;

  long mappedVal;
  if (val > centerVal)
    mappedVal = map(val, centerVal + DEAD_ZONE, JOY_MAX, 16385, 32767);
  else
    mappedVal = map(val, JOY_MIN, centerVal - DEAD_ZONE, 0, 16383);

  mappedVal = constrain(mappedVal, 0, 32767);
  return invert ? (32767 - mappedVal) : mappedVal;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BT_LED, OUTPUT);

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  bleGamepadConfig.setVid(0xe502);
  bleGamepadConfig.setPid(0xabcd);
  bleGamepad.begin(&bleGamepadConfig);
}

unsigned long lastBlinkTime = 0;
bool ledState = false;

void loop() {
  unsigned long now = millis();

  if (bleGamepad.isConnected()) {
    digitalWrite(BT_LED, HIGH);  // Solid LED

    // === JOYSTICK READ & CALIBRATE ===
    int rawLX = analogRead(JOY1_X);
    int rawLY = analogRead(JOY1_Y);
    int rawRX = analogRead(JOY2_X);
    int rawRY = analogRead(JOY2_Y);

    uint16_t joyLX = calibrateAxis(rawLX, JOY1_X_CENTER);
    uint16_t joyLY = calibrateAxis(rawLY, JOY1_Y_CENTER, true);
    uint16_t joyRX = calibrateAxis(rawRX, JOY2_X_CENTER);
    uint16_t joyRY = calibrateAxis(rawRY, JOY2_Y_CENTER, true);

    // === Smoothing ===
    joyLX = prevLX + (joyLX - prevLX) * SMOOTHING_FACTOR;
    joyLY = prevLY + (joyLY - prevLY) * SMOOTHING_FACTOR;
    joyRX = prevRX + (joyRX - prevRX) * SMOOTHING_FACTOR;
    joyRY = prevRY + (joyRY - prevRY) * SMOOTHING_FACTOR;

    if (abs(joyLX - prevLX) > 100 || abs(joyLY - prevLY) > 100)
      bleGamepad.setLeftThumb(joyLX, joyLY);
    if (abs(joyRX - prevRX) > 100 || abs(joyRY - prevRY) > 100)
      bleGamepad.setRightThumb(joyRX, joyRY);

    prevLX = joyLX; prevLY = joyLY;
    prevRX = joyRX; prevRY = joyRY;

    // === BUTTONS ===
    for (int i = 0; i < NUM_BUTTONS; i++) {
      bool pressed = !digitalRead(buttonPins[i]);
      if (pressed != lastButtonStates[i]) {
        if (pressed) {
          bleGamepad.press(gamepadButtons[i]);
          Serial.printf("PRESSED  - GPIO %d -> Gamepad Button %d\n", buttonPins[i], gamepadButtons[i]);
        } else {
          bleGamepad.release(gamepadButtons[i]);
          Serial.printf("RELEASED - GPIO %d -> Gamepad Button %d\n", buttonPins[i], gamepadButtons[i]);
        }
        lastButtonStates[i] = pressed;
      }
    }

    bleGamepad.sendReport();
    Serial.printf("L: %d,%d R: %d,%d\n", joyLX, joyLY, joyRX, joyRY);
  } else {
    // === BT Not Connected - Fast Blink ===
    if (now - lastBlinkTime > 200) {
      ledState = !ledState;
      digitalWrite(BT_LED, ledState);
      lastBlinkTime = now;
    }
  }

  delay(10);
}
//Rohanshaj K R