// Use when Teensy Wire1 directly drives the wrist-band PCA9685.
// Do not initialize Wire/Serial1 or issue any motor commands from this board.
void setup() {
  pinMode(11, INPUT);  // SDA: high impedance, no internal pull-up
  pinMode(12, INPUT);  // SCL: high impedance, no internal pull-up
  Serial.begin(115200);
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    Serial.println("#band-passive Teensy owns PCA9685; MKR I2C disabled");
  }
}
