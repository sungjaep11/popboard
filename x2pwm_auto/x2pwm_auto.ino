// x2pwm_auto.ino
// ---------------------------------------------------------------------------
// x2pwm() 캘리브레이션용 STANDALONE 스케치 (LK-Navigator 자동기록 버전).
//
// PWM을 자동으로 계단 스윕(수동 입력 없음). 각 PWM을 일정 시간 유지하므로,
// LK-Navigator에서 변위가 "계단 모양"으로 기록됩니다. 나중에 각 계단(plateau)
// 값만 읽어서 PWM과 짝지으면 됩니다.
//
// 실행 순서:
//   1) LK-G 이동평균을 4096으로 (진동 평균 -> 계단이 평평하게).
//   2) LK-Navigator에서 "기록(REC)" 시작.
//   3) 이 스케치 업로드 -> 자동으로 스윕 -> 끝나면 REC 정지.
//   4) LK-Navigator에서 계단 값들 읽기(또는 CSV export).
// ---------------------------------------------------------------------------

// ==== Bar(actuator) 핀 -- vcm-bars.ino와 동일 ==============================
#define pinEN     23   // PWM/enable  (Bar2:23, Bar1:19)
#define pinPH     22   // phase/방향   (Bar2:22, Bar1:20)
#define pinSLEEP  21   // 드라이버 sleep (HIGH=깨어있음)

// ==== 스윕 파라미터 =========================================================
#define PWM_MAX_VAL   4095
#define PWM_STEP      64    // 자동이라 촘촘히 가능(128 -> 한 방향 33점)
#define PH_LEVEL      HIGH   // 미는 방향. 반대는 LOW로 재실행
#define DWELL_MS      2500   // 각 PWM 유지 시간(계단이 또렷/안정되게)
#define START_DELAY   3000   // 시작 전 대기(REC 켤 시간 확보)

// ---------------------------------------------------------------------------

void doStep(int pwm){
  analogWrite(pinEN, pwm);
  // 경과시간과 PWM을 같이 찍어둠(LK-Navigator 로그와 정렬용 참고)
  //Serial.print(millis());
  Serial.print("\tPWM=");
  Serial.println(pwm);
  delay(DWELL_MS);
}

void setup(){
  Serial.begin(115200);
  while(!Serial && millis() < 3000);

  pinMode(pinEN, OUTPUT);
  pinMode(pinPH, OUTPUT);
  pinMode(pinSLEEP, OUTPUT);

  digitalWrite(pinEN, LOW);
  digitalWrite(pinSLEEP, HIGH);         // 드라이버 켜기
  digitalWrite(pinPH, PH_LEVEL);        // 방향 고정

  analogWriteFrequency(pinEN, 24000);   // 24 kHz
  analogWriteResolution(12);            // 0..4095

  Serial.print("# 자동 스윕. 방향(PH)=");
  Serial.println(PH_LEVEL == HIGH ? "HIGH" : "LOW");
  Serial.println("# LK-Navigator REC 켜두세요. 5초 후 시작...");
  delay(START_DELAY);

  Serial.println("# t_ms\tPWM  (계단 순서대로 LK-Navigator 값에 대응)");

  // 올림만 (0 -> 4095)
  for (int pwm = 0; pwm <= PWM_MAX_VAL; pwm += PWM_STEP) doStep(pwm);

  analogWrite(pinEN, 0);                 // 끝나면 0으로 복귀
  digitalWrite(pinSLEEP, LOW);          // 드라이버 재우기
  Serial.println("# done -- LK-Navigator REC 정지하세요");
}

void loop(){
  // 스윕은 setup()에서 1회. 여기선 할 일 없음.
}
