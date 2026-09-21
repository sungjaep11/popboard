#include "MyPCA9685.h"

MyPCA9685 module;

constexpr uint8_t PCA9685_ADDRESS = 0x60;
constexpr uint32_t PCA9685_I2C_HZ = 100000;
constexpr uint16_t DEFAULT_VIBRATION_HZ = 250;
constexpr uint16_t DEFAULT_VIBRATION_AMP = 1000;  // 0..2047
constexpr uint32_t DEFAULT_VIBRATION_MS = 50;

uint16_t vibrationHz = DEFAULT_VIBRATION_HZ;
uint16_t vibrationAmp = DEFAULT_VIBRATION_AMP;
uint32_t vibrationMs = DEFAULT_VIBRATION_MS;
bool continuousMode = false;
bool pca9685Ready = false;
uint32_t lastPca9685RetryMs = 0;

enum Hand : uint8_t { LEFT_HAND, RIGHT_HAND };
enum LocalDirection : uint8_t { DIR_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN };

uint8_t pca9685Status() {
  Wire.beginTransmission(PCA9685_ADDRESS);
  return Wire.endTransmission();
}

bool waitForPca9685(uint32_t timeoutMs) {
  uint32_t startedAt = millis();
  do {
    if (pca9685Status() == 0) return true;
    delay(20);
  } while (millis() - startedAt < timeoutMs);
  return false;
}

void printPca9685Status() {
  uint8_t status = pca9685Status();
  Serial.print("#band-i2c address=0x");
  Serial.print(PCA9685_ADDRESS, HEX);
  Serial.print(" status=");
  Serial.print(status == 0 ? "ok" : "error");
  Serial.print(" code=");
  Serial.println(status);
}

void scanI2cBus() {
  uint8_t found = 0;
  // Scan slowly so long wiring or weak pull-ups do not hide an otherwise
  // correctly addressed device. Restore the normal bus speed afterwards.
  Wire.setClock(PCA9685_I2C_HZ);
  Serial.println("#band-i2c-scan begin");
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t status = Wire.endTransmission();
    if (status != 0) continue;
    Serial.print("#band-i2c-found address=0x");
    if (address < 0x10) Serial.print('0');
    Serial.println(address, HEX);
    ++found;
  }
  Serial.print("#band-i2c-scan end found=");
  Serial.println(found);
  Wire.setClock(PCA9685_I2C_HZ);
}

// Rows are left/right hands; columns are L/U/R/D. These defaults assume
// channels 0..3 are the left band and 4..7 are the right band. Run the bridge
// with --test-motors and reorder this table if the physical positions differ.
const uint8_t MOTOR_FOR_HAND_DIRECTION[2][4] = {
  {1, 0, 3, 2},  // left wrist:  L, U, R, D
  {7, 4, 5, 6}   // right wrist: L, U, R, D
};

bool motorOn[8] = {};
uint32_t motorOffAt[8] = {};
char uartCommandBuffer[48];
uint8_t uartCommandLength = 0;
char usbCommandBuffer[48];
uint8_t usbCommandLength = 0;

void stopMotor(uint8_t motor) {
  if (pca9685Ready) module.setAmplitude(motor, 0);
  motorOn[motor] = false;
}

void stopAllMotors() {
  for (uint8_t motor = 0; motor < 8; ++motor) stopMotor(motor);
}

bool initializePca9685() {
  if (pca9685Status() != 0) return false;
  module.begin(&Wire, PCA9685_ADDRESS, vibrationHz);
  pca9685Ready = (pca9685Status() == 0);
  if (pca9685Ready) {
    stopAllMotors();
    Serial.println("#band-i2c-ready");
  }
  return pca9685Ready;
}

void servicePca9685() {
  if (pca9685Ready) return;
  uint32_t now = millis();
  if (now - lastPca9685RetryMs < 500) return;
  lastPca9685RetryMs = now;
  initializePca9685();
}

void stopHand(Hand hand) {
  uint8_t first = hand == LEFT_HAND ? 0 : 4;
  for (uint8_t motor = first; motor < first + 4; ++motor) stopMotor(motor);
}

void printConfig() {
  Serial.print("#band-cfg freq=");
  Serial.print(vibrationHz);
  Serial.print(" amp=");
  Serial.print(vibrationAmp);
  Serial.print(" ms=");
  Serial.print(vibrationMs);
  Serial.print(" mode=");
  Serial.println(continuousMode ? "continuous" : "pulse");
  Serial.print("#band-bus hz=");
  Serial.println(PCA9685_I2C_HZ);
  printPca9685Status();
}

void startMotor(uint8_t motor, uint16_t amp) {
  if (motor >= 8) return;
  if (!pca9685Ready && !initializePca9685()) {
    Serial.println("#band-error i2c-not-ready");
    return;
  }

  if (amp > 2047) amp = 2047;
  module.setAmplitude(motor, amp);
  motorOn[motor] = true;
  motorOffAt[motor] = continuousMode ? 0 : millis() + vibrationMs;

  Serial.print("#motor-hit motor=");
  Serial.println(motor);
}

void startMotor(uint8_t motor) {
  startMotor(motor, vibrationAmp);
}

void startHandMask(Hand hand, uint8_t mask, uint16_t amp) {
  if (hand > RIGHT_HAND || mask == 0 || mask > 0x0F) return;
  // One continuous direction per hand. Keep the other wrist independent.
  if (continuousMode) stopHand(hand);
  for (uint8_t direction = 0; direction < 4; ++direction) {
    if (mask & (1u << direction)) {
      startMotor(MOTOR_FOR_HAND_DIRECTION[hand][direction], amp);
    }
  }
  Serial.print("#band-hit hand=");
  Serial.print(hand == LEFT_HAND ? 'L' : 'R');
  Serial.print(" mask=");
  Serial.println(mask);
}

void updateMotors() {
  if (continuousMode) return;
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

  if (strcmp(line, "GET") == 0) {
    printConfig();
    return;
  }

  if (strcmp(line, "SCAN") == 0) {
    scanI2cBus();
    return;
  }

  if (strcmp(line, "STOP") == 0) {
    stopAllMotors();
    Serial.println("#band-ok stopped");
    return;
  }

  if (strncmp(line, "RELEASE ", 8) == 0) {
    char handChar = line[8];
    if ((handChar != 'L' && handChar != 'R') || line[9] != '\0') {
      Serial.println("#band-error expected=RELEASE_L_or_R");
      return;
    }
    // A release must not shorten a fixed pulse; it only ends held feedback.
    if (continuousMode) stopHand(handChar == 'L' ? LEFT_HAND : RIGHT_HAND);
    Serial.print("#band-release hand=");
    Serial.println(handChar);
    return;
  }

  if (strncmp(line, "SET ", 4) == 0) {
    char *name = line + 4;
    char *space = strchr(name, ' ');
    if (!space) {
      Serial.println("#band-error expected=SET_name_value");
      return;
    }
    *space = '\0';
    char *value = space + 1;
    char *end = nullptr;

    if (strcmp(name, "FREQ") == 0) {
      long v = strtol(value, &end, 10);
      if (end == value || *end != '\0' || v < 24 || v > 1500) {
        Serial.println("#band-error freq=24..1500");
        return;
      }
      vibrationHz = (uint16_t)v;
      if (pca9685Ready) module.setFrequency(vibrationHz);
    } else if (strcmp(name, "AMP") == 0) {
      long v = strtol(value, &end, 10);
      if (end == value || *end != '\0' || v < 0 || v > 2047) {
        Serial.println("#band-error amp=0..2047");
        return;
      }
      vibrationAmp = (uint16_t)v;
      for (uint8_t motor = 0; motor < 8; ++motor) {
        if (motorOn[motor]) module.setAmplitude(motor, vibrationAmp);
      }
    } else if (strcmp(name, "MS") == 0) {
      unsigned long v = strtoul(value, &end, 10);
      if (end == value || *end != '\0' || v < 1 || v > 60000) {
        Serial.println("#band-error ms=1..60000");
        return;
      }
      vibrationMs = (uint32_t)v;
    } else if (strcmp(name, "MODE") == 0) {
      bool nextContinuous;
      if (strcmp(value, "PULSE") == 0) nextContinuous = false;
      else if (strcmp(value, "CONTINUOUS") == 0) nextContinuous = true;
      else {
        Serial.println("#band-error mode=PULSE_or_CONTINUOUS");
        return;
      }
      if (continuousMode != nextContinuous) stopAllMotors();
      continuousMode = nextContinuous;
    } else {
      Serial.println("#band-error unknown-setting");
      return;
    }
    printConfig();
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
  uint16_t amp = vibrationAmp;
  if (*end == ' ') {
    char *ampEnd = nullptr;
    long requestedAmp = strtol(end + 1, &ampEnd, 10);
    if (ampEnd == end + 1 || *ampEnd != '\0' || requestedAmp < 0 || requestedAmp > 2047) {
      Serial.println("#band-error amp=0..2047");
      return;
    }
    amp = (uint16_t)requestedAmp;
  } else if (*end != '\0') {
    Serial.println("#band-error expected=BAND_hand_mask_optionalAmp");
    return;
  }
  startHandMask(handChar == 'L' ? LEFT_HAND : RIGHT_HAND, (uint8_t)mask, amp);
}

void readCommandsFrom(Stream &input, char *buffer, uint8_t &length) {
  while (input.available()) {
    char ch = (char)input.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      buffer[length] = '\0';
      processCommand(buffer);
      length = 0;
    } else if (length < 47) {
      buffer[length++] = ch;
    } else {
      length = 0;
    }
  }
}

void readCommands() {
  // Teensy commands arrive on the hardware UART. USB stays available for
  // manual motor tests, configuration, and PC-side diagnostics.
  readCommandsFrom(Serial1, uartCommandBuffer, uartCommandLength);
  readCommandsFrom(Serial, usbCommandBuffer, usbCommandLength);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);

  Wire.begin();
  Wire.setClock(PCA9685_I2C_HZ);
  // With separately powered boards, the PCA9685 rail can become valid after
  // the MKR has already entered setup(). Do not lose its one-time init writes.
  if (waitForPca9685(2000)) initializePca9685();

  Serial.println("#band-ready");
  printConfig();
}

void loop() {
  servicePca9685();
  readCommands();
  updateMotors();
}
