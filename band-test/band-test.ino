#include "MyPCA9685.h"

MyPCA9685 module;

constexpr uint8_t PCA9685_ADDRESS = 0x60;
constexpr uint16_t VIBRATION_HZ = 250;
constexpr uint16_t VIBRATION_AMP = 1000;  // 0..2047
constexpr uint32_t VIBRATION_MS = 50;

enum Hand : uint8_t { LEFT_HAND, RIGHT_HAND };
enum LocalDirection : uint8_t { DIR_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN };

// Rows are left/right hands; columns are L/U/R/D. These defaults assume
// channels 0..3 are the left band and 4..7 are the right band. Run the bridge
// with --test-motors and reorder this table if the physical positions differ.
const uint8_t MOTOR_FOR_HAND_DIRECTION[2][4] = {
  {1, 0, 3, 2},  // left wrist:  L, U, R, D
  {7, 4, 5, 6}   // right wrist: L, U, R, D
};

bool motorOn[8] = {};
uint32_t motorOffAt[8] = {};
char commandBuffer[32];
uint8_t commandLength = 0;

void stopMotor(uint8_t motor) {
  module.setAmplitude(motor, 0);
  motorOn[motor] = false;
}

void startMotor(uint8_t motor) {
  if (motor >= 8) return;

  module.setAmplitude(motor, VIBRATION_AMP);
  motorOn[motor] = true;
  motorOffAt[motor] = millis() + VIBRATION_MS;

  Serial.print("#motor-hit motor=");
  Serial.println(motor);
}

void startHandMask(Hand hand, uint8_t mask) {
  if (hand > RIGHT_HAND || mask == 0 || mask > 0x0F) return;
  for (uint8_t direction = 0; direction < 4; ++direction) {
    if (mask & (1u << direction)) {
      startMotor(MOTOR_FOR_HAND_DIRECTION[hand][direction]);
    }
  }
  Serial.print("#band-hit hand=");
  Serial.print(hand == LEFT_HAND ? 'L' : 'R');
  Serial.print(" mask=");
  Serial.println(mask);
}

void updateMotors() {
  uint32_t now = millis();
  for (uint8_t motor = 0; motor < 8; ++motor) {
    if (motorOn[motor] && (int32_t)(now - motorOffAt[motor]) >= 0) {
      stopMotor(motor);
    }
  }
}

void processCommand(char *line) {
  // Normal command: "BAND L 1". Mask bits are L/U/R/D = 1/2/4/8.
  // vcm-tune normally sends exactly one bit. "TEST 0" tests one raw channel.
  if (strncmp(line, "TEST ", 5) == 0) {
    char *end = nullptr;
    long motor = strtol(line + 5, &end, 10);
    if (end != line + 5 && motor >= 0 && motor < 8) startMotor((uint8_t)motor);
    return;
  }

  if (strncmp(line, "BAND ", 5) != 0) return;
  char handChar = line[5];
  if ((handChar != 'L' && handChar != 'R') || line[6] != ' ') {
    Serial.println("#band-error expected=BAND_L_or_R_mask");
    return;
  }

  char *end = nullptr;
  long mask = strtol(line + 7, &end, 10);
  if (end == line + 7 || mask < 1 || mask > 15) {
    Serial.println("#band-error expected=mask_1..15");
    return;
  }
  startHandMask(handChar == 'L' ? LEFT_HAND : RIGHT_HAND, (uint8_t)mask);
}

void readCommands() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      commandBuffer[commandLength] = '\0';
      processCommand(commandBuffer);
      commandLength = 0;
    } else if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = ch;
    } else {
      commandLength = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(1000000);
  module.begin(&Wire, PCA9685_ADDRESS, VIBRATION_HZ);

  for (uint8_t motor = 0; motor < 8; ++motor) stopMotor(motor);
  Serial.println("#band-ready");
}

void loop() {
  readCommands();
  updateMotors();
}
