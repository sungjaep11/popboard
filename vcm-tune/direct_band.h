#pragma once
#include <Arduino.h>
#include <Wire.h>

// Photo wiring: Teensy 16=SCL1, 17=SDA1, PCA9685 address 0x60.
// The MKR must run band-passive: only one controller owns this bus.
class DirectBand {
public:
  void begin() {
    Wire1.setSCL(16);
    Wire1.setSDA(17);
    Wire1.begin();
    Wire1.setClock(100000);
    initialize();
  }

  void service() {
    if (!ready_) {
      if ((uint32_t)(millis() - retryAt_) >= 500) initialize();
      return;
    }
    for (uint8_t m = 0; m < 8; ++m) {
      if (timed_[m] && (int32_t)(millis() - offAt_[m]) >= 0) {
        if (!amplitude(m, 0)) return;
        timed_[m] = false;
      }
    }
  }

  void hit(bool left, uint8_t mask, uint16_t amp, bool continuous) {
    if (!ready_ || !(mask & 0x0F) || amp == 0) return;
    if (continuous) release(left);
    const uint8_t mapping[2][4] = {{1, 0, 3, 2}, {7, 4, 5, 6}};
    for (uint8_t d = 0; d < 4; ++d)
      if (mask & (1u << d)) start(mapping[left ? 0 : 1][d], amp, continuous);
  }

  void release(bool left) {
    for (uint8_t m = left ? 0 : 4; m < (left ? 4 : 8); ++m) {
      if (!ready_ || !amplitude(m, 0)) return;
      timed_[m] = false;
    }
  }

  void test(uint8_t motor, uint16_t amp = 1000) { start(motor, amp, false); }
  uint16_t frequency = 250;
  uint32_t duration = 50;
  uint32_t cooldown = 0;  // Continuous-mode idle timeout; 0 keeps feedback on.
  void setFrequency(uint16_t hz) { frequency = hz; initialize(); }

  void status() {
    Serial.print("#band-direct bus=Wire1 scl=16 sda=17 address=0x60 ready=");
    Serial.print(ready_ ? 1 : 0);
    Serial.print(" code="); Serial.println(lastError_);
  }

private:
  bool ready_ = false;
  bool timed_[8] = {};
  uint32_t offAt_[8] = {};
  uint32_t retryAt_ = 0;
  uint8_t lastError_ = 0;

  bool write(uint8_t reg, const uint8_t* data, uint8_t count) {
    Wire1.beginTransmission(0x60);
    Wire1.write(reg);
    Wire1.write(data, count);
    lastError_ = Wire1.endTransmission();
    if (!lastError_) return true;
    if (ready_) {
      Serial.print("#band-direct i2c-error code="); Serial.println(lastError_);
    }
    ready_ = false;
    retryAt_ = millis();
    return false;
  }

  bool reg(uint8_t address, uint8_t value) { return write(address, &value, 1); }

  void initialize() {
    retryAt_ = millis();
    ready_ = false;
    // Internal 25 MHz oscillator; round the configured PWM prescaler.
    if (!reg(0x00, 0x10) || !reg(0xFE, (uint8_t)(25000000.0f / (4096.0f * frequency) + 0.5f - 1)) || !reg(0x01, 0x04)) return;
    // Enable auto-increment while still asleep, clear every channel, then wake.
    if (!reg(0x00, 0x30)) return;
    const uint8_t off[] = {0, 0, 0, 0x10};
    if (!write(0xFA, off, sizeof(off)) || !reg(0x00, 0x20)) return;
    delayMicroseconds(500);
    if (!reg(0x00, 0xA0)) return;
    for (uint8_t m = 0; m < 8; ++m) timed_[m] = false;
    ready_ = true;
    status();
  }

  bool amplitude(uint8_t motor, uint16_t amp) {
    if (amp > 2047) amp = 2047;
    // Two opposite phases per motor, identical to MyPCA9685::setAmplitude.
    const uint16_t secondOff = 2048 + amp;
    const uint8_t values[] = {
      0, 0, (uint8_t)amp, (uint8_t)(amp ? amp >> 8 : 0x10),
      0, (uint8_t)(amp ? 8 : 0), (uint8_t)(amp ? secondOff : 0),
      (uint8_t)(amp ? secondOff >> 8 : 0x10)
    };
    return write(0x06 + 8 * motor, values, sizeof(values));
  }

  void start(uint8_t motor, uint16_t amp, bool continuous) {
    if (!ready_ || motor >= 8 || !amplitude(motor, amp)) return;
    // Continuous hits arrive on contact/down or meaningful position changes,
    // not every sensor frame. Movement restarts this timer and wakes an idle band.
    timed_[motor] = !continuous || cooldown > 0;
    offAt_[motor] = millis() + (continuous ? cooldown : duration);
  }
};
