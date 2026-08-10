#include <USBHost_t36.h>
#include "sensel.h"
#include "sensel_register_map.h"
#include "keymap.h"
#include "barstate.h"

// Sensel interface //////////////////////////////////////////////////////////////

// Teensy 4.0 USB Host 및 시리얼 객체 생성
USBHost myusb;
USBSerial senselSerial(myusb); 

// 라이브러리 내부에서 사용하는 버퍼 구조체 정의
SenselFrame frame;

void initializeSensel(){
  // USB Host 가동
  myusb.begin();

  // Open serial for SenselSerial declared in sensel.h
  senselOpen();

  // Set frame content to scan. No pressure or label support.
  senselSetFrameContent(SENSEL_REG_CONTACTS_FLAG);

  // // 1. 프레임 레이트를 500Hz로 설정 (0x20 주소, 2바이트)
  // // 반환값이 없는 void 함수이므로 변수에 대입하지 않고 '단독'으로 호출합니다.
  // unsigned char rate_val[2] = {0xF4, 0x01}; 
  // senselWriteReg(SENSEL_REG_SCAN_FRAME_RATE, 2, rate_val);
  // Serial.println("▶ 프레임 레이트(0x20) 명령 전송 완료");

  // // 2. 스캔 디테일을 고속 모드로 변경 (0x23 주소, 1바이트)
  // unsigned char detail_val = 2; 
  // senselWriteReg(SENSEL_REG_SCAN_DETAIL_CONTROL, 1, &detail_val);
  // Serial.println("▶ 스캔 디테일(0x23) 명령 전송 완료");
  // Serial.println("------------------------------------------");

  // Start scanning the Sensel device
  senselStartScanning();
}

// Haptic output interface ////////////////////////////////////////////////////////

#define pinEN1 19 // PWM (FlexPWM1)
#define pinPH1 20
#define pinEN2 23 // PWM (FlexPWM4)
#define pinPH2 22
#define pinSLEEP 21


#define X2PWM_N 64
const uint16_t x2pwm_lut[X2PWM_N + 1] = {
   320,  901, 1296, 1585, 1826, 2034, 2213, 2355, 2456, 2516, 2554, 2584, 2613,
  2644, 2677, 2707, 2736, 2765, 2792, 2819, 2848, 2877, 2907, 2938, 2965, 2992,
  3019, 3047, 3075, 3105, 3134, 3163, 3191, 3218, 3244, 3270, 3299, 3327, 3357,
  3386, 3413, 3439, 3466, 3493, 3520, 3550, 3580, 3608, 3635, 3662, 3689, 3716,
  3747, 3777, 3806, 3835, 3862, 3889, 3918, 3948, 3978, 4009, 4039, 4067, 4095
};

float x2pwm(float x){
  if (x <= 0.0f) return 0.0f;
  if (x >= 1.0f) return x2pwm_lut[X2PWM_N];
  float f = x * X2PWM_N;                       // 명령을 표 위치(0~64)로 환산
  int   i = (int)f;                            // 정수부분
  float t = f - i;                             // 소수부분 = 그 칸 안에서 얼마나 왔나(0~1).

  return x2pwm_lut[i] + t * (x2pwm_lut[i + 1] - x2pwm_lut[i]);
}

void moveBar1(float x) {
  if (x >= 0) {
    digitalWrite(pinPH1, HIGH);
    analogWrite(pinEN1, x2pwm(x));
  } else {
    digitalWrite(pinPH1, LOW);
    analogWrite(pinEN1, x2pwm(-x));
  }
}

void moveBar2(float x) {
  if (x >= 0) {
    digitalWrite(pinPH2, HIGH);
    analogWrite(pinEN2, x2pwm(x));
  } else {
    digitalWrite(pinPH2, LOW);
    analogWrite(pinEN2, x2pwm(-x));
  }
}

void initializeBars(){
  pinMode(pinEN1, OUTPUT);  
  pinMode(pinPH1, OUTPUT);
  pinMode(pinEN2, OUTPUT);  
  pinMode(pinPH2, OUTPUT);
  pinMode(pinSLEEP, OUTPUT);

  digitalWrite(pinEN1, LOW);
  digitalWrite(pinEN2, LOW);
  digitalWrite(pinSLEEP, HIGH);

  // Set the PWM frequency to 24kHz
  // For 24kHz, 12bit resolution is the maximum that the teensy can have.
  analogWriteFrequency(pinEN1, 24000);
  analogWriteFrequency(pinEN2, 24000);
  analogWriteResolution(12);
}

// MAIN //////////////////////////////////////////////////////////////////////////

// the force-displacement curve
#define F_PEAK    100 // in Sensel unit
#define F_VALLEY  70  // in Sensel unit
#define F_END     90  // in Sensel unit
#define Z_PEAK    0.50  // 0 - 1 (duty)
#define Z_VALLEY  0.80  // 0 - 1 (duty)
#define Z_END     1.0  // 0 - 1 (duty)


#define CLICK_SCALE 0.5f

// 프레임 읽기가 연속으로 이만큼 실패하면 손을 뗀 것으로 간주하고 바를 놓는다.
#define MAX_FRAME_DROPS 10

// 좌/우 독립 상태 (struct SideState 정의는 barstate.h)
SideState sideL = {{{0, 0.0f, -1}, {0, 0.0f, -1}}};
SideState sideR = {{{0, 0.0f, -1}, {0, 0.0f, -1}}};

int           frameDrops = 0;   // 연속 실패 횟수
unsigned long dropTotal  = 0;   // 누적 실패 횟수 (진단용)

// 접촉별 키 판정 결과. keyAt()이 전체 키를 훑으므로 프레임당 한 번만 계산해 둔다.
int   ctKey[SENSEL_MAX_CONTACTS];    // 눌린 키 인덱스 (-1: 키캡 밖)
float ctGain[SENSEL_MAX_CONTACTS];   // 키 안에서의 위치 gain (0~1)
bool  ctLeft[SENSEL_MAX_CONTACTS];   // true: 왼쪽 바 담당
int   nContacts = 0;                 // 이번 프레임의 유효 접촉 개수(배열 범위)

// 이번 프레임의 모든 접촉을 키/게인/담당 손으로 분류한다.
void classifyContacts() {
  nContacts = frame.n_contacts;
  if (nContacts > SENSEL_MAX_CONTACTS) nContacts = SENSEL_MAX_CONTACTS;   // 파서가 걸러주지만 이중 방어

  for (int i = 0; i < nContacts; i++) {
    float x = frame.contacts[i].x_pos;
    ctKey[i]  = keyAt(x, frame.contacts[i].y_pos, &ctGain[i]);
    ctLeft[i] = keyIsLeft(ctKey[i], x);
  }
}

// 1: 매 루프 상세 로그(좌/우 키·힘·구동값) 출력
#define VERBOSE_LOG 1

// 1: 1초마다 프레임 성공/실패 원인 요약 출력 (진단용)
// 드롭을 측정할 때는 VERBOSE_LOG를 0으로 두고 이것만 켤 것.
// 초당 200줄의 상세 로그가 그 자체로 루프 시간을 잡아먹어 측정값을 왜곡한다.
#define SENSEL_STATS 0

// 실패 원인별 카운터 (SenselStatus 인덱스) + 1초마다 요약 출력
unsigned long statusCount[SENSEL_N_STATUS] = {0};
elapsedMillis statsTimer;

const char* STATUS_NAME[SENSEL_N_STATUS] = {
  "ok", "hdrTO", "badAck", "badSize", "payTO", "badContent", "lenMismatch"
};

void printSenselStats() {
  Serial.print("[sensel/s]");
  for (int i = 0; i < SENSEL_N_STATUS; i++) {
    Serial.print("  ");
    Serial.print(STATUS_NAME[i]);
    Serial.print("=");
    Serial.print(statusCount[i]);
    statusCount[i] = 0;
  }
  Serial.println();
}

// force -> 단위 버클 곡선값(0~Z_END). 좌굴 상태는 접촉(슬롯)마다 따로 갱신된다.
// 호출부에서 CLICK_SCALE * keyGain을 곱해 실제 바 변위 밴드로 환산한다.
float forceToZ(ContactState* c, float force) {
  float z;

  if (c->buttonState == 0) {   // before buckling
    if (force < F_PEAK) {
      z = force / F_PEAK * Z_PEAK;
    }
    else {
      c->buttonState = 1;
      z = Z_END;
    }
  }
  else {                       // after buckling
    if (force > F_VALLEY) {
      z = (force - F_END) * (Z_END - Z_VALLEY) / (F_END - F_VALLEY) + Z_END;
      if (z > Z_END) z = Z_END;
    }
    else {
      c->buttonState = 0;
      z = force / F_PEAK * Z_PEAK;
    }
  }

  return z;
}

// 한쪽 손(leftSide)의 접촉을 슬롯 2개(s->c[0..1])에 배정한다.
// outIdx[k]: 슬롯 k가 담당할 접촉의 프레임 인덱스(없으면 -1).
// 빈 슬롯에는 아직 배정 안 된 접촉 중 제일 센 것을 채운다.
void assignSlots(SideState* s, bool leftSide, int outIdx[SIDE_SLOTS]) {
  for (int k = 0; k < SIDE_SLOTS; k++) outIdx[k] = -1;

  // 1) 직전 프레임에 담당하던 접촉이 아직 이쪽에 살아 있으면 그 슬롯을 유지한다.
  for (int k = 0; k < SIDE_SLOTS; k++) {
    if (s->c[k].activeId < 0) continue;
    for (int i = 0; i < nContacts; i++) {
      byte t = frame.contacts[i].type;
      if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
      if (ctLeft[i] != leftSide) continue;
      if ((int)frame.contacts[i].id == s->c[k].activeId) { 
        outIdx[k] = i;
        break; 
      }
    }
    if (outIdx[k] < 0) {                 // 손을 뗌
      s->c[k].activeId    = -1;
      s->c[k].buttonState = 0;
      s->c[k].prevForce   = 0.0f;
    }
  }

  // 2) 빈 슬롯을 아직 배정 안 된 접촉 중 제일 센 것으로 채운다.
  for (int k = 0; k < SIDE_SLOTS; k++) {
    if (outIdx[k] >= 0) continue;
    int best = -1;                       // 지금까지 찾은 제일 센 접촉의 프레임 인덱스 (-1: 아직 없음)
    float bestF = 0.0f;                  // 그 접촉의 total force
    for (int i = 0; i < nContacts; i++) {
      byte t = frame.contacts[i].type;
      if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
      if (ctLeft[i] != leftSide) continue;

      bool taken = false;                // 다른 슬롯이 이미 가져간 접촉은 제외
      for (int j = 0; j < SIDE_SLOTS; j++) {
        if (outIdx[j] == i) { 
          taken = true; 
          break; 
        }
      }
        if (taken) continue;

      float f = frame.contacts[i].total_force;
      if (best < 0 || f > bestF) {
        best = i;
        bestF = f;
      }
    }
    if (best >= 0) {
      outIdx[k] = best;
      s->c[k].activeId = (int)frame.contacts[best].id;
    }
  }
}

// 한쪽 손의 슬롯 2개로 바 구동값을 계산한다. 각 슬롯은 자기 버클 곡선을
// keyOut[k]: 슬롯 k가 누른 키 인덱스(없으면 -1), 로그용.
float sideDrive(SideState* s, const int idx[SIDE_SLOTS], int keyOut[SIDE_SLOTS]) {
  float z = 0.0f;

  for (int k = 0; k < SIDE_SLOTS; k++) {
    float force   = 0.0f;
    float keyGain = 0.0f;   // 키캡 밖(갭)이거나 접촉 없음 -> 햅틱 0
    keyOut[k] = -1;

    if (idx[k] >= 0) {
      force     = frame.contacts[idx[k]].total_force;
      keyOut[k] = ctKey[idx[k]];
      keyGain   = ctGain[idx[k]];
    }

    // 접촉이 없어도(force=0) 호출해서 버클링 상태를 풀어줘야 한다.
    float unit = forceToZ(&s->c[k], force);
    s->c[k].prevForce = force;

    z += CLICK_SCALE * unit * keyGain;
  }

  return z;
}

elapsedMillis loopTimer;
#define LOOP_PERIOD 5  // maybe 5 ms is the max due to the sensel data acquisition time.

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  initializeSensel();

  initializeBars();

  // Initialize timers and state variables
  loopTimer = 0;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    sideL.c[k].buttonState = 0;  sideL.c[k].prevForce = 0.0f;  sideL.c[k].activeId = -1;
    sideR.c[k].buttonState = 0;  sideR.c[k].prevForce = 0.0f;  sideR.c[k].activeId = -1;
  }
  frame.n_contacts = 0;
  nContacts  = 0;
  frameDrops = 0;
  dropTotal  = 0;
  statsTimer = 0;
  for (int i = 0; i < SENSEL_N_STATUS; i++) statusCount[i] = 0;
}

void loop() {
  if (loopTimer >= LOOP_PERIOD) {
    loopTimer = 0;

    // 읽기에 실패하면 직전 프레임을 그대로 유지한다. 매번 0으로 밀면
    // 드롭 한 번에 구동값이 0으로 떨어졌다 복귀하면서 바가 튄다.
    if (senselGetFrame(&frame)) {
      frameDrops = 0;
    }
    else {
      frameDrops++;
      dropTotal++;
      if (frameDrops >= MAX_FRAME_DROPS) frame.n_contacts = 0;
    }
#if SENSEL_STATS
    statusCount[senselLastStatus]++;
    if (statsTimer >= 1000) {
      statsTimer = 0;
      printSenselStats();
    }
#endif

    // 접촉별 키/담당 손을 먼저 정한 뒤, 각 손에서 대표 접촉을 하나씩 고른다.
    classifyContacts();

    int idxL[SIDE_SLOTS], idxR[SIDE_SLOTS];
    assignSlots(&sideL, true,  idxL);
    assignSlots(&sideR, false, idxR);

    // 두 바를 매 프레임 각각 갱신 -> 양손 동시 입력이 서로를 막지 않는다.
    int   kiL[SIDE_SLOTS], kiR[SIDE_SLOTS];
    float zL = sideDrive(&sideL, idxL, kiL);
    float zR = sideDrive(&sideR, idxR, kiR);

    moveBar1(zL);
    moveBar2(zR);

#if VERBOSE_LOG
    // 기본 감지 정보: 매 루프 출력 (좌/우 슬롯별 키/힘, 바 구동값)
    Serial.print("L key=");
    Serial.print(kiL[0] >= 0 ? KEYS[kiL[0]].label : "-");
    Serial.print(",");
    Serial.print(kiL[1] >= 0 ? KEYS[kiL[1]].label : "-");
    Serial.print("\tf=");
    Serial.print(sideL.c[0].prevForce);
    Serial.print(",");
    Serial.print(sideL.c[1].prevForce);
    Serial.print("\tdrive=");
    Serial.print(zL);

    Serial.print("\t|\tR key=");
    Serial.print(kiR[0] >= 0 ? KEYS[kiR[0]].label : "-");
    Serial.print(",");
    Serial.print(kiR[1] >= 0 ? KEYS[kiR[1]].label : "-");
    Serial.print("\tf=");
    Serial.print(sideR.c[0].prevForce);
    Serial.print(",");
    Serial.print(sideR.c[1].prevForce);
    Serial.print("\tdrive=");
    Serial.println(zR);
#endif
  }
}