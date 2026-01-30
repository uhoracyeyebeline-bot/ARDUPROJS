#include <Arduino.h>
#include <SoftwareSerial.h> // For Bluetooth HC-05/HC-06

// ================== PINOUT ==================
#define X_STEP_PIN 2
#define X_DIR_PIN 3
#define X_ENABLE_PIN 4

#define Y_STEP_PIN 5
#define Y_DIR_PIN 6
#define Y_ENABLE_PIN 7

#define Z_STEP_PIN 8
#define Z_DIR_PIN 9
#define Z_ENABLE_PIN 10

#define BUZZER_PIN 13 // Buzzer moved to digital pin 13

#define JOYSTICK1_VRx A0 // Left Joystick X-axis
#define JOYSTICK1_VRy A1 // Left Joystick Y-axis
#define JOYSTICK1_SW 11  // Left Joystick button (moved to pin 11)

#define JOYSTICK2_VRy A3 // Right Joystick Y-axis
#define JOYSTICK2_SW 12  // Right Joystick button (moved to pin 12)

// Bluetooth on A4 (TX) and A5 (RX) via SoftwareSerial
#define BT_TX_PIN A4 // Arduino TX -> HC-05 RX (use voltage divider to 3.3V)
#define BT_RX_PIN A5 // Arduino RX <- HC-05 TX

// ================== MOTION SETTINGS ==================
#define STEP_PULSE_US 5        // Step pin high pulse width
#define STEP_DELAY_US 700      // Fixed delay between steps for non-ramped moves
#define MIN_STEP_DELAY_US 200  // Ramping minimum delay (faster)
#define MAX_STEP_DELAY_US 1500 // Ramping maximum delay (slower)
#define JOG_STEPS 50           // Baseline steps per jog command
#define GEAR_RATIO 4.875       // Gear ratio multiplier

// Joystick tuning
#define CENTER_POSITION 512    // Joystick center analog value
#define JOYSTICK_THRESHOLD 50  // Deadzone threshold
#define JOY_FILTER_ALPHA 0.2f  // Low-pass filter alpha (0..1) for smoothing
#define JOY_MIN_RAW_STEPS 8    // Minimum raw steps per cycle from joystick deflection
#define JOY_MAX_RAW_STEPS 26   // Maximum raw steps per cycle from joystick deflection

// Timing control to avoid repeated large bursts
#define JOYSTICK_REPEAT_MS 50  // Minimum interval between joystick-triggered moves

// Buzzer
#define BUZZER_FREQUENCY 3000  // Buzzer frequency in Hz

// ================== AXIS STRUCT ==================
struct Axis {
  uint8_t stepPin;
  uint8_t dirPin;
  uint8_t enPin;
};

// ================== AXES ==================
Axis X = {X_STEP_PIN, X_DIR_PIN, X_ENABLE_PIN};
Axis Y = {Y_STEP_PIN, Y_DIR_PIN, Y_ENABLE_PIN};
Axis Z = {Z_STEP_PIN, Z_DIR_PIN, Z_ENABLE_PIN};

// ================== STATES ==================
enum Mode { MANUAL, AUTO };
Mode currentMode = MANUAL; // Start in MANUAL mode

// ================== BLUETOOTH ==================
SoftwareSerial Bluetooth(BT_RX_PIN, BT_TX_PIN); // RX, TX

// ================== FILTERED JOYSTICK VALUES ==================
float joy1X_filtered = CENTER_POSITION;
float joy1Y_filtered = CENTER_POSITION;
float joy2Y_filtered = CENTER_POSITION;

unsigned long lastJoy1MoveMs = 0;
unsigned long lastJoy2MoveMs = 0;

// ================== UTILITIES ==================
long convertSteps(long rawSteps) {
  // Convert raw steps by gear ratio to actual driver steps
  return (long)(rawSteps * GEAR_RATIO);
}

// Clamp helper
template <typename T>
T clamp(T v, T lo, T hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

// Map joystick deflection to a small step count (smooth, non-violent)
long stepsFromDeflection(int deflection) {
  // deflection is abs(analog - CENTER) - threshold
  deflection = clamp(deflection, 0, 512 - JOYSTICK_THRESHOLD);
  // Scale to [JOY_MIN_RAW_STEPS .. JOY_MAX_RAW_STEPS]
  long raw = JOY_MIN_RAW_STEPS +
             (long)((JOY_MAX_RAW_STEPS - JOY_MIN_RAW_STEPS) *
                    ((float)deflection / (float)(512 - JOYSTICK_THRESHOLD)));
  return raw;
}

// ================== PROTOTYPES ==================
void startBuzzer();
void stopBuzzer();
void moveAxis(Axis a, bool dir, long steps);
void rampMove(Axis a, bool dir, long steps);
void moveYPlusAndZPlus(long steps);
void stopAll();
void toggleAutoMode();
void handleJoystickControls();
void handleBluetoothControls();
void handleContinuousManual();
void autoModeSequence();

// ================== BUZZER ==================
void startBuzzer() { tone(BUZZER_PIN, BUZZER_FREQUENCY); }
void stopBuzzer()  { noTone(BUZZER_PIN); }

// ================== MOVES ==================
void moveAxis(Axis a, bool dir, long steps) {
  digitalWrite(a.enPin, LOW);
  digitalWrite(a.dirPin, dir ? HIGH : LOW);

  startBuzzer();
  for (long i = 0; i < steps; i++) {
    digitalWrite(a.stepPin, HIGH);
    delayMicroseconds(STEP_PULSE_US);
    digitalWrite(a.stepPin, LOW);
    delayMicroseconds(STEP_DELAY_US);
  }
  stopBuzzer();
}

void rampMove(Axis a, bool dir, long steps) {
  uint16_t delayDuration = MAX_STEP_DELAY_US;
  uint16_t decrement = (MAX_STEP_DELAY_US - MIN_STEP_DELAY_US) / (steps > 0 ? steps : 1);

  digitalWrite(a.enPin, LOW);
  digitalWrite(a.dirPin, dir ? HIGH : LOW);

  startBuzzer();
  for (long i = 0; i < steps; i++) {
    digitalWrite(a.stepPin, HIGH);
    delayMicroseconds(delayDuration);
    digitalWrite(a.stepPin, LOW);
    delayMicroseconds(delayDuration);

    if (delayDuration > MIN_STEP_DELAY_US) {
      delayDuration -= decrement;
      if (delayDuration < MIN_STEP_DELAY_US) delayDuration = MIN_STEP_DELAY_US;
    }
  }
  stopBuzzer();
}

void moveYPlusAndZPlus(long steps) {
  // Simultaneous fixed stepping with small steps per cycle to avoid violent motion
  digitalWrite(Y.enPin, LOW);
  digitalWrite(Z.enPin, LOW);
  digitalWrite(Y.dirPin, HIGH); // Y+
  digitalWrite(Z.dirPin, HIGH); // Z+

  startBuzzer();
  for (long i = 0; i < steps; i++) {
    digitalWrite(Y.stepPin, HIGH);
    digitalWrite(Z.stepPin, HIGH);
    delayMicroseconds(STEP_PULSE_US);
    digitalWrite(Y.stepPin, LOW);
    digitalWrite(Z.stepPin, LOW);
    delayMicroseconds(STEP_DELAY_US);
  }
  stopBuzzer();
}

// ================== CONTROL HELPERS ==================
void stopAll() {
  digitalWrite(X_ENABLE_PIN, HIGH);
  digitalWrite(Y_ENABLE_PIN, HIGH);
  digitalWrite(Z_ENABLE_PIN, HIGH);
  stopBuzzer();
  Serial.println("STOPPED: All motors disabled.");
  Bluetooth.println("STOPPED: All motors disabled.");
}

void toggleAutoMode() {
  currentMode = (currentMode == MANUAL) ? AUTO : MANUAL;
  const char* msg = (currentMode == MANUAL) ? "Switched to MANUAL mode" : "Switched to AUTO mode";
  Serial.println(msg);
  Bluetooth.println(msg);
}

// ================== INPUT HANDLERS ==================
void handleJoystickControls() {
  // Low-pass filter the analog inputs for smooth command generation
  int joy1X_raw = analogRead(JOYSTICK1_VRx);
  int joy1Y_raw = analogRead(JOYSTICK1_VRy);
  int joy2Y_raw = analogRead(JOYSTICK2_VRy);

  joy1X_filtered = joy1X_filtered + JOY_FILTER_ALPHA * (joy1X_raw - joy1X_filtered);
  joy1Y_filtered = joy1Y_filtered + JOY_FILTER_ALPHA * (joy1Y_raw - joy1Y_filtered);
  joy2Y_filtered = joy2Y_filtered + JOY_FILTER_ALPHA * (joy2Y_raw - joy2Y_filtered);

  bool joystick1Active = (digitalRead(JOYSTICK1_SW) == LOW); // Left joystick button
  bool joystick2Active = (digitalRead(JOYSTICK2_SW) == LOW); // Right joystick button

  unsigned long now = millis();

  // Joystick 1 controls X and Z (requires button pressed to be active)
  if (joystick1Active && (now - lastJoy1MoveMs) >= JOYSTICK_REPEAT_MS) {
    int xDef = (int)fabs(joy1X_filtered - CENTER_POSITION) - JOYSTICK_THRESHOLD;
    int zDef = (int)fabs(joy1Y_filtered - CENTER_POSITION) - JOYSTICK_THRESHOLD;

    if (xDef > 0) {
      bool dirX = (joy1X_filtered > CENTER_POSITION);
      long rawSteps = stepsFromDeflection(xDef);
      long steps = convertSteps(rawSteps);
      // Use ramp for smoother start
      rampMove(X, dirX, steps);
      lastJoy1MoveMs = now;
    } else if (zDef > 0) {
      bool dirZ = (joy1Y_filtered > CENTER_POSITION);
      long rawSteps = stepsFromDeflection(zDef);
      long steps = convertSteps(rawSteps);
      rampMove(Z, dirZ, steps);
      lastJoy1MoveMs = now;
    }
  }

  // Joystick 2 controls Y and the combo Y+&Z+ (requires button pressed)
  if (joystick2Active && (now - lastJoy2MoveMs) >= JOYSTICK_REPEAT_MS) {
    int yDef = (int)fabs(joy2Y_filtered - CENTER_POSITION) - JOYSTICK_THRESHOLD;

    if (yDef > 0) {
      bool up = (joy2Y_filtered > CENTER_POSITION);
      long rawSteps = stepsFromDeflection(yDef);
      long steps = convertSteps(rawSteps);

      // Smooth behavior: small steps each cycle; avoid violent long bursts
      if (up) {
        // Combined smooth movement upward: Y+ and Z+ with small steps
        moveYPlusAndZPlus(steps);
      } else {
        // Backward on Y only (Y-)
        rampMove(Y, false, steps);
      }
      lastJoy2MoveMs = now;
    }
  }
}

void handleBluetoothControls() {
  if (Bluetooth.available()) {
    char c = toupper(Bluetooth.read());
    switch (c) {
      case 'W': moveAxis(Z, true, convertSteps(JOG_STEPS));  break; // Z+
      case 'S': moveAxis(Z, false, convertSteps(JOG_STEPS)); break; // Z-
      case 'A': moveAxis(X, false, convertSteps(JOG_STEPS)); break; // X-
      case 'D': moveAxis(X, true, convertSteps(JOG_STEPS));  break; // X+
      case 'Q': moveYPlusAndZPlus(convertSteps(JOG_STEPS));  break; // Y+ & Z+ (small bursts)
      case 'E': moveAxis(Y, false, convertSteps(JOG_STEPS)); break; // Y-
      case 'G': toggleAutoMode(); break;
      default: Bluetooth.println("Invalid Command"); break;
    }
  }
}

void handleContinuousManual() {
  if (Serial.available()) {
    char c = toupper(Serial.read());
    switch (c) {
      case 'W': moveAxis(Z, true, convertSteps(JOG_STEPS));  break; // Z+
      case 'S': moveAxis(Z, false, convertSteps(JOG_STEPS)); break; // Z-
      case 'A': moveAxis(X, false, convertSteps(JOG_STEPS)); break; // X-
      case 'D': moveAxis(X, true, convertSteps(JOG_STEPS));  break; // X+
      case 'Q': moveYPlusAndZPlus(convertSteps(JOG_STEPS));  break; // Y+ & Z+
      case 'E': moveAxis(Y, false, convertSteps(JOG_STEPS)); break; // Y-
      case 'G': toggleAutoMode(); break;
      // Optional demo: ramped Z+ for testing smoothness
      case 'R': rampMove(Z, true, convertSteps(JOG_STEPS));  break;
      default: break;
    }
  }
  handleJoystickControls();
}

// ================== AUTO MODE ==================
void autoModeSequence() {
  Serial.println("Auto Mode Starting...");
  Bluetooth.println("Auto Mode Starting...");

  for (int i = 0; i < 5; i++) {
    rampMove(Y, true, convertSteps(40)); // Y+ smooth
    delay(300);
    rampMove(Z, true, convertSteps(40)); // Z+ smooth
    delay(300);
  }

  Serial.println("Auto Mode Finished");
  Bluetooth.println("Auto Mode Finished");
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  Bluetooth.begin(9600);

  pinMode(X_STEP_PIN, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT);
  pinMode(X_ENABLE_PIN, OUTPUT);

  pinMode(Y_STEP_PIN, OUTPUT);
  pinMode(Y_DIR_PIN, OUTPUT);
  pinMode(Y_ENABLE_PIN, OUTPUT);

  pinMode(Z_STEP_PIN, OUTPUT);
  pinMode(Z_DIR_PIN, OUTPUT);
  pinMode(Z_ENABLE_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(JOYSTICK1_SW, INPUT_PULLUP); // pin 11
  pinMode(JOYSTICK2_SW, INPUT_PULLUP); // pin 12

  // Enable drivers
  digitalWrite(X_ENABLE_PIN, LOW);
  digitalWrite(Y_ENABLE_PIN, LOW);
  digitalWrite(Z_ENABLE_PIN, LOW);

  // Initialize filtered values
  joy1X_filtered = CENTER_POSITION;
  joy1Y_filtered = CENTER_POSITION;
  joy2Y_filtered = CENTER_POSITION;

  Serial.println("Robot Arm Initialized: Keyboard, Joysticks, Bluetooth Ready.");
  Bluetooth.println("Bluetooth Ready. Send W/A/S/D/Q/E or G.");
}

// ================== LOOP ==================
void loop() {
  handleBluetoothControls();

  if (Serial.available()) {
    char c = toupper(Serial.read());
    if (c == 'G') toggleAutoMode();
  }

  if (currentMode == MANUAL) {
    handleContinuousManual();
  } else {
    autoModeSequence();
  }
}
