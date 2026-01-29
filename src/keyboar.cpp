// This file implements a Keyboard HID example — only compile on boards
// that support native USB HID (AVR32U4-based boards like Leonardo).
#if defined(USBCON)

#include <Arduino.h>
#include <Keyboard.h>

char letters[16] = {
  'a','b','c','d',
  'e','f','g','h',
  'i','j','k','l',
  'm','n','o','p'
};

int letterPins[16] = {
  2,3,4,5,6,7,8,9,
  10,11,12,13,A0,A1,A2,A3
};

#define UP_PIN     A4
#define DOWN_PIN   A5
#define LEFT_PIN   A6
#define RIGHT_PIN  A7

#define DBL1_PIN   A8
#define DBL2_PIN   A9

unsigned long lastClick1 = 0;
unsigned long lastClick2 = 0;
const int doubleClickTime = 400;

void setup() {
  Keyboard.begin();

  for (int i = 0; i < 16; i++) {
    pinMode(letterPins[i], INPUT_PULLUP);
  }

  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);

  pinMode(DBL1_PIN, INPUT_PULLUP);
  pinMode(DBL2_PIN, INPUT_PULLUP);
}

void loop() {

  // ---- Normal buttons (a–p) ----
  for (int i = 0; i < 16; i++) {
    if (digitalRead(letterPins[i]) == LOW) {
      Keyboard.write(letters[i]);
      delay(200); // debounce
    }
  }

  // ---- Direction buttons (q–t) ----
  if (digitalRead(UP_PIN) == LOW) {
    Keyboard.write('q');
    delay(200);
  }

  if (digitalRead(DOWN_PIN) == LOW) {
    Keyboard.write('r');
    delay(200);
  }

  if (digitalRead(LEFT_PIN) == LOW) {
    Keyboard.write('s');
    delay(200);
  }

  if (digitalRead(RIGHT_PIN) == LOW) {
    Keyboard.write('t');
    delay(200);
  }

  // ---- Double click Y ----
  if (digitalRead(DBL1_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastClick1 < doubleClickTime) {
      Keyboard.write('y');
    }
    lastClick1 = now;
    delay(200);
  }

  // ---- Double click Z ----
  if (digitalRead(DBL2_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastClick2 < doubleClickTime) {
      Keyboard.write('z');
    }
    lastClick2 = now;
    delay(200);
  }
}

#endif // USBCON
