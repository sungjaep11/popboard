//GH: Based on the Adafruit PWMServoDriver library. (230723)
#include "MyPCA9685.h"

/*!
 *  @brief  Instantiates a new PCA9685 PWM driver chip with the I2C address on a
 * TwoWire interface
 *  @param  addr The 7-bit I2C address to locate this chip, default is 0x40
 */
MyPCA9685::MyPCA9685()
{
  _i2c = NULL;
  _i2caddr = 0;
}

/*!
 *  @brief  Setups the I2C interface and hardware
 *  @param  prescale
 *          Sets External Clock (Optional)
 */
void MyPCA9685::begin(TwoWire *i2c, uint8_t addr, uint16_t freq) 
{
  _i2c = i2c;
  _i2caddr = addr;

  reset();
  setPWMFreq(freq);

  for(uint8_t i = 0; i < 16; i++)
    turnOff(i);
}

/*!
 *  @brief  Sends a reset command to the PCA9685 chip over I2C
 */
void MyPCA9685::reset() {
  write8(PCA9685_MODE1, MODE1_RESTART);
  delay(10);  //GH: required? no harm anyway.
}

/*!
 *  @brief  Sets the PWM frequency for the entire chip, up to ~1.6 KHz
 *  @param  freq Floating point frequency that we will attempt to match
 */
void MyPCA9685::setPWMFreq(uint16_t freq) {
  // Range output modulation frequency is dependant on oscillator
  if (freq < 1)
    freq = 1;
  if (freq > 3500)
    freq = 3500; // Datasheet limit is 3052=50MHz/(4*4096)

  float prescaleval = ((FREQUENCY_OSCILLATOR / (freq * 4096.0)) + 0.5) - 1;
  if (prescaleval < PCA9685_PRESCALE_MIN)
    prescaleval = PCA9685_PRESCALE_MIN;
  if (prescaleval > PCA9685_PRESCALE_MAX)
    prescaleval = PCA9685_PRESCALE_MAX;
  uint8_t prescale = (uint8_t)prescaleval;

  uint8_t oldmode = read8(PCA9685_MODE1);
  uint8_t newmode = (oldmode & ~MODE1_RESTART) | MODE1_SLEEP; // sleep
  write8(PCA9685_MODE1, newmode);                             // go to sleep
  write8(PCA9685_PRESCALE, prescale); // set the prescaler
  write8(PCA9685_MODE1, oldmode);
  delay(5);
  // This sets the MODE1 register to turn on auto increment.
  write8(PCA9685_MODE1, oldmode | MODE1_RESTART | MODE1_AI);
}

/*!
 *  @brief  Sets the output mode of the PCA9685 to either
 *  open drain or push pull / totempole.
 *  Warning: LEDs with integrated zener diodes should
 *  only be driven in open drain mode.
 *  @param  totempole Totempole if true, open drain if false.
 */
void MyPCA9685::setOutputMode(bool totempole) {
  uint8_t oldmode = read8(PCA9685_MODE2);
  uint8_t newmode;
  if (totempole) {
    newmode = oldmode | MODE2_OUTDRV;
  } else {
    newmode = oldmode & ~MODE2_OUTDRV;
  }
  write8(PCA9685_MODE2, newmode);
}

//GH: added
/*!
 *  @brief  Turns off the servo (if the servo respect this signal)
 *  @param  ch One of the PWM output channels, from 0 to 15
 */
void MyPCA9685::turnOff(uint8_t ch){
  // Special values for signal fully off.
  setPWM(ch, 0, 4096);
}

//GH: added
/*!
 *  @brief  sets the amplitude of the vibration
 *  @param  i One of the motors, from 0 to 7
 *  @param  amp the vibration amplitude, from 0 to 2047
 */
void MyPCA9685::setAmplitude(uint8_t i, uint16_t amp){
  if(i >= 8) return;
  if(amp == 0){
    turnOff(2 * i);
    turnOff(2 * i + 1);
    return;
  }
  if(amp > 2047) amp = 2047;
  setPWM(2 * i, 0, amp);
  setPWM(2 * i + 1, 2048, amp + 2048);
}

/*!
 *  @brief  Sets the PWM output of one of the PCA9685 pins
 *  @param  ch One of the PWM output channels, from 0 to 15
 *  @param  on At what point in the 4096-part cycle to turn the PWM output ON
 *  @param  off At what point in the 4096-part cycle to turn the PWM output OFF
 */
void MyPCA9685::setPWM(uint8_t ch, uint16_t on, uint16_t off) {
  _i2c->beginTransmission(_i2caddr);
  _i2c->write(PCA9685_LED0_ON_L + 4 * ch);
  _i2c->write(on);
  _i2c->write(on >> 8);
  _i2c->write(off);
  _i2c->write(off >> 8);
  _i2c->endTransmission();
}

/******************* Low level I2C interface */
uint8_t MyPCA9685::read8(uint8_t addr) {
  _i2c->beginTransmission(_i2caddr);
  _i2c->write(addr);
  _i2c->endTransmission();

  _i2c->requestFrom((uint8_t)_i2caddr, (uint8_t)1);
  return _i2c->read();
}

void MyPCA9685::write8(uint8_t addr, uint8_t d) {
  _i2c->beginTransmission(_i2caddr);
  _i2c->write(addr);
  _i2c->write(d);
  _i2c->endTransmission();
}
