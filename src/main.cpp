

#include <Arduino.h>

// =======================================================
// PINS
// =======================================================

// X axis
const uint8_t X_STEP_PIN   = 2;
const uint8_t X_DIR_PIN    = 3;
const uint8_t X_ENABLE_PIN = 4;

// Y axis
const uint8_t Y_STEP_PIN   = 5;
const uint8_t Y_DIR_PIN    = 6;
const uint8_t Y_ENABLE_PIN = 7;

// Z axis
const uint8_t Z_STEP_PIN   = 8;
const uint8_t Z_DIR_PIN    = 9;
const uint8_t Z_ENABLE_PIN = 10;

// Joystick 1 (X & Y movement)
const uint8_t JOY1_X  = A0;   // Left / Right  → X
const uint8_t JOY1_Y  = A1;   // Forward / Back → Y
const uint8_t JOY1_SW = 11;   // STOP

// Joystick 2 (Z movement)
const uint8_t JOY2_Y  = A3;   // Up / Down → Z
const uint8_t JOY2_SW = 12;   // AUTO

// =======================================================
// MOTION SETTINGS
// =======================================================

const unsigned long STEP_PULSE_US = 4;
const unsigned long START_DELAY_US = 2000;
const unsigned long FAST_DELAY_US  = 400;

const int JOY_CENTER  = 512;
const int JOY_DEADZONE = 60;

const long JOG_RAW_STEPS = 5;

// Gear ratio = 39 / 8 = 4.875
static long convertSteps(long steps) {
  return (steps * 39 + 4) / 8;
}

// =======================================================
// MODE
// =======================================================

enum Mode { MANUAL, AUTO };
static Mode currentMode = MANUAL;

// =======================================================
// AXIS CLASS
// =======================================================

class Axis {
public:
  Axis(uint8_t s, uint8_t d, uint8_t e)
    : stepPin(s), dirPin(d), enPin(e) {}

  void begin() {
    pinMode(stepPin, OUTPUT);
    pinMode(dirPin, OUTPUT);
    pinMode(enPin, OUTPUT);
    digitalWrite(stepPin, LOW);
    digitalWrite(dirPin, LOW);
    disable();
  }

  void enable()  { digitalWrite(enPin, LOW); }
  void disable() { digitalWrite(enPin, HIGH); }

  void move(bool dir, long steps, unsigned long delayUs) {
    if (steps <= 0) return;

    enable();
    digitalWrite(dirPin, dir ? HIGH : LOW);

    for (long i = 0; i < steps; i++) {
      digitalWrite(stepPin, HIGH);
      delayMicroseconds(STEP_PULSE_US);
      digitalWrite(stepPin, LOW);
      delayMicroseconds(delayUs);
    }
  }

private:
  uint8_t stepPin, dirPin, enPin;
};

// =======================================================
// AXIS OBJECTS
// =======================================================

Axis X(X_STEP_PIN, X_DIR_PIN, X_ENABLE_PIN);
Axis Y(Y_STEP_PIN, Y_DIR_PIN, Y_ENABLE_PIN);
Axis Z(Z_STEP_PIN, Z_DIR_PIN, Z_ENABLE_PIN);

// =======================================================
// HELPERS
// =======================================================

static void stopAll() {
  Serial.println("STOPPED (holding position)");
}

static unsigned long joystickSpeed(int value) {
  return map(abs(value - JOY_CENTER),
             JOY_DEADZONE, 512,
             START_DELAY_US, FAST_DELAY_US);
}

// =======================================================
// JOYSTICK CONTROL
// =======================================================

static void handleJoystick() {
  int joyX = analogRead(JOY1_X);
  int joyY = analogRead(JOY1_Y);
  int joyZ = analogRead(JOY2_Y);

  // ---------- X AXIS ----------
  if (abs(joyX - JOY_CENTER) > JOY_DEADZONE) {
    bool dir = joyX > JOY_CENTER;
    X.move(dir, convertSteps(JOG_RAW_STEPS),
           joystickSpeed(joyX));
  }

  // ---------- Y AXIS ----------
  if (abs(joyY - JOY_CENTER) > JOY_DEADZONE) {
    bool dir = joyY > JOY_CENTER;
    Y.move(dir, convertSteps(JOG_RAW_STEPS),
           joystickSpeed(joyY));
  }

  // ---------- Z AXIS ----------
  if (abs(joyZ - JOY_CENTER) > JOY_DEADZONE) {
    bool dir = joyZ > JOY_CENTER;
    Z.move(dir, convertSteps(JOG_RAW_STEPS),
           joystickSpeed(joyZ));
  }

  // ---------- BUTTONS ----------
  if (digitalRead(JOY1_SW) == LOW) {
    stopAll();
  }

  if (digitalRead(JOY2_SW) == LOW) {
    currentMode = AUTO;
  }
}

// =======================================================
// AUTO SEQUENCE
// =======================================================

static void autoSequence() {
  Serial.println("AUTO MODE START");

  Z.move(true,  convertSteps(400), 800);
  delay(500);

  X.move(true,  convertSteps(800), 800);
  delay(500);

  Y.move(true,  convertSteps(600), 800);
  delay(500);

  X.move(false, convertSteps(800), 800);
  delay(500);

  Y.move(false, convertSteps(600), 800);
  delay(500);

  Z.move(false, convertSteps(400), 800);
  delay(500);

  Serial.println("AUTO MODE END");
}

// =======================================================
// SETUP & LOOP
// =======================================================

void setup() {
  Serial.begin(115200);

  X.begin();
  Y.begin();
  Z.begin();

  pinMode(JOY1_SW, INPUT_PULLUP);
  pinMode(JOY2_SW, INPUT_PULLUP);

  Serial.println("Robot Arm Ready");
  Serial.println("Joystick 1: X/Y movement");
  Serial.println("Joystick 2: Z movement");
}

void loop() {
  if (currentMode == MANUAL) {
    handleJoystick();
  } else {
    autoSequence();
    currentMode = MANUAL;
  }
}


















// #include <Arduino.h>



// // Improved, idiomatic C++ rewrite of the original Arduino sketch.
// // - Better structure using a small Axis class
// // - Safer Serial input handling
// // - Clear initialization of enable pins
// // - Same external behavior: manual jog commands + auto sequence

// // ================== PINS ==================
// const uint8_t X_STEP_PIN = 2;
// const uint8_t X_DIR_PIN  = 3;
// const uint8_t X_ENABLE_PIN = 4;

// const uint8_t Y_STEP_PIN = 5;
// const uint8_t Y_DIR_PIN  = 6;
// const uint8_t Y_ENABLE_PIN = 7;

// const uint8_t Z_STEP_PIN = 8;
// const uint8_t Z_DIR_PIN  = 9;
// const uint8_t Z_ENABLE_PIN = 10;

// const uint8_t LED_PIN = 13;

// // ================== MOTION SETTINGS ==================
// const unsigned long STEP_PULSE_US = 4UL;            // STEP high time (us)
// const unsigned long START_STEP_DELAY_US = 2000UL;  // initial period between steps (us)
// const unsigned long TARGET_STEP_DELAY_US = 400UL;  // final (fast) period (us)
// const long JOG_STEPS = 20;                        // base jog steps
// const double GEAR_RATIO = 4.875;                  // gear ratio (39:8)

// // ================== MODE ==================
// // Use plain enum for compatibility with older toolchains
// enum Mode { MANUAL, AUTO };
// static Mode currentMode = MANUAL;

// // ================== UTILS ==================
// static long convertSteps(long rawSteps) {
//   // Use integer math to avoid floating on AVR: GEAR_RATIO = 39/8 = 4.875
//   // round to nearest: (rawSteps * 39 + 4) / 8
//   return (rawSteps * 39 + 4) / 8;
// }

// // ================== Axis class ==================
// class Axis {
// public:
//   Axis(uint8_t step, uint8_t dir, uint8_t en) : stepPin(step), dirPin(dir), enPin(en) {}

//   void begin() {
//     pinMode(stepPin, OUTPUT);
//     pinMode(dirPin, OUTPUT);
//     pinMode(enPin, OUTPUT);
//     digitalWrite(stepPin, LOW);
//     digitalWrite(dirPin, LOW);
//     // leave enabled pin controlled by caller
//   }

//   void enable() { digitalWrite(enPin, LOW); }
//   void disable() { digitalWrite(enPin, HIGH); }

//   // Blocking move with simple linear acceleration (ramp) between startPeriod -> endPeriod
//   void move(bool direction, long steps,
//             unsigned long startPeriod = START_STEP_DELAY_US,
//             unsigned long endPeriod = TARGET_STEP_DELAY_US,
//             unsigned long pulseUs = STEP_PULSE_US) {
//     if (steps <= 0) return;

//     enable();
//     digitalWrite(dirPin, direction ? HIGH : LOW);

//     for (long i = 0; i < steps; ++i) {
//       // progress [0..1]
//       float progress = (steps > 1) ? (static_cast<float>(i) / static_cast<float>(steps - 1)) : 1.0f;
//       float periodF = (1.0f - progress) * static_cast<float>(startPeriod) + progress * static_cast<float>(endPeriod);
//       unsigned long period = static_cast<unsigned long>(periodF + 0.5f);

//       if (period <= pulseUs) period = pulseUs + 1;

//       digitalWrite(stepPin, HIGH);
//       delayMicroseconds(pulseUs);
//       digitalWrite(stepPin, LOW);
//       delayMicroseconds(period - pulseUs);
//     }
//   }

// private:
//   const uint8_t stepPin;
//   const uint8_t dirPin;
//   const uint8_t enPin;
// };

// // ================== Axis instances ==================
// static Axis X(X_STEP_PIN, X_DIR_PIN, X_ENABLE_PIN);
// static Axis Y(Y_STEP_PIN, Y_DIR_PIN, Y_ENABLE_PIN);
// static Axis Z(Z_STEP_PIN, Z_DIR_PIN, Z_ENABLE_PIN);

// // ================== High-level helpers ==================
// static void enableMotors() {
//   X.enable();
//   Y.enable();
//   Z.enable();
//   Serial.println("Motors enabled");
// }

// static void disableMotors() {
//   X.disable();
//   Y.disable();
//   Z.disable();
//   Serial.println("Motors disabled");
// }

// static void stopAll() {
//   // For stepper drivers, keep the motors enabled to hold position unless user requests otherwise.
//   Serial.println("Motors stopping (holding position)");
// }

// // ================== Auto sequence ==================
// static void autoModeSequence() {
//   Serial.println("Starting AUTO mode...");
//   enableMotors();

//   Z.move(true,  convertSteps(400));
//   delay(500);

//   X.move(true,  convertSteps(800));
//   delay(500);

//   Y.move(true,  convertSteps(600));
//   delay(500);

//   X.move(false, convertSteps(800));
//   delay(500);

//   Y.move(false, convertSteps(600));
//   delay(500);

//   Z.move(false, convertSteps(400));
//   delay(500);

//   Serial.println("AUTO mode complete");
// }

// // ================== Manual command handling ==================
// static void handleManual() {
//   if (!Serial.available()) return;

//   int inByte = Serial.read();
//   if (inByte < 0) return;
//   char cmd = static_cast<char>(toupper(static_cast<unsigned char>(inByte)));

//   switch (cmd) {
//     case 'W': // +Z
//       Z.move(true, convertSteps(JOG_STEPS));
//       Serial.println("OK Z+");
//       break;
//     case 'S': // -Z
//       Z.move(false, convertSteps(JOG_STEPS));
//       Serial.println("OK Z-");
//       break;
//     case 'A': // -X
//       X.move(false, convertSteps(JOG_STEPS));
//       Serial.println("OK X-");
//       break;
//     case 'D': // +X
//       X.move(true, convertSteps(JOG_STEPS));
//       Serial.println("OK X+");
//       break;
//     case 'Q': // +Y
//       Y.move(true, convertSteps(JOG_STEPS));
//       Serial.println("OK Y+");
//       break;
//     case 'E': // -Y
//       Y.move(false, convertSteps(JOG_STEPS));
//       Serial.println("OK Y-");
//       break;
//     case 'X': // STOP/HOLD
//       stopAll();
//       Serial.println("STOPPED");
//       break;
//     case 'G': // AUTO
//       currentMode = Mode::AUTO;
//       Serial.println("Switching to AUTO mode...");
//       break;
//     default:
//       Serial.println("Unknown command");
//       break;
//   }
// }

// // ================== Arduino setup/loop ==================
// void setup() {
//   Serial.begin(115200);

//   // Initialize axis pins
//   X.begin();
//   Y.begin();
//   Z.begin();

//   pinMode(LED_PIN, OUTPUT);

//   // Make enable pins inactive first to avoid accidental drive during startup
//   X.disable();
//   Y.disable();
//   Z.disable();

//   // Then enable (explicit)
//   enableMotors();

//   Serial.println("Robot Arm Control Initialized!");
//   Serial.println("Commands: W/S (Z), A/D (X), Q/E (Y), X (Stop), G (Auto Mode)");
// }

// void loop() {
//   if (currentMode == Mode::MANUAL) {
//     handleManual();
//   } else {
//     autoModeSequence();
//     currentMode = Mode::MANUAL;
//     Serial.println("Auto sequence complete. Returning to MANUAL mode.");
//   }
// }
