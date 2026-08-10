// =============================================================================
//  keyboard  —  vcm-fj-texture + 실제 USB HID 키보드 출력
// -----------------------------------------------------------------------------
//  vcm-fj-texture(F/J 진동 텍스처 랜드마크)를 그대로 두고, 버클링(=촉각 클릭)을
//  PC로 가는 USB 키 입력으로 내보낸다. 키가 들어가는 시점과 손가락이 클릭을
//  느끼는 시점이 정확히 같다.
//
//  ★ Tools > USB Type = "Serial + Keyboard + Mouse + Joystick" 필수.
//    (Keyboard 단독으로 하면 Serial 디버그가 죽는다. 반드시 Serial 포함본으로.)
//
//  Teensy 4.0은 USB 디바이스 포트(PC로 가는 마이크로 USB)와 USB 호스트 포트가
//  별개다. Sensel은 호스트 쪽에 물려 있으므로 디바이스 포트로 HID를 낼 수 있다.
//
//  키 출력 방식: "엣지"가 아니라 "상태 재구성".
//   - 5ms마다 지금 버클링 중인 슬롯을 전부 스캔해 HID 리포트를 처음부터 다시 만든다.
//   - press/release 엣지를 쫓지 않으므로 릴리즈를 놓쳐 키가 고착되는 일이 없다.
//     접촉이 사라지든 프레임이 드롭되든 그 키는 다음 리포트에서 그냥 빠진다.
//   - 리포트가 바뀔 때만 전송한다. 200Hz로 매번 쏘면 USB와 호스트를 낭비한다.
//
//  안전장치 (HID는 오동작하면 PC에 문자를 쏟아붓는다):
//   - ENABLE_HID       : 컴파일 타임 차단
//   - HID_BOOT_LOCKOUT_MS : 부팅 직후 무입력 구간
//   - Serial로 'h'     : 런타임 on/off 토글
//   폭주하면 Teensy의 물리 Program 버튼을 누르고 재플래시하면 된다.
//   첫 테스트는 반드시 빈 텍스트 에디터에 포커스를 두고 할 것. 터미널/IDE 금지.
// =============================================================================

#include <USBHost_t36.h>

#if !defined(KEYBOARD_INTERFACE)
  #error "Tools > USB Type 을 'Serial + Keyboard + Mouse + Joystick' 으로 설정하세요."
#endif

#include "sensel.h"
#include "sensel_register_map.h"
#include "keymap.h"
#include "barstate.h"

// Sensel interface //////////////////////////////////////////////////////////////
USBHost myusb;
USBSerial senselSerial(myusb);
SenselFrame frame;

void initializeSensel(){
  myusb.begin();
  senselOpen();
  senselSetFrameContent(SENSEL_REG_CONTACTS_FLAG);
  senselStartScanning();
}

// Haptic output interface ////////////////////////////////////////////////////////
#define pinEN1 19
#define pinPH1 20
#define pinEN2 23
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
  float f = x * X2PWM_N;
  int   i = (int)f;
  float t = f - i;
  return x2pwm_lut[i] + t * (x2pwm_lut[i + 1] - x2pwm_lut[i]);
}

void moveBar1(float x) {
  if (x >= 0) { digitalWrite(pinPH1, HIGH); analogWrite(pinEN1, x2pwm(x)); }
  else        { digitalWrite(pinPH1, LOW);  analogWrite(pinEN1, x2pwm(-x)); }
}
void moveBar2(float x) {
  if (x >= 0) { digitalWrite(pinPH2, HIGH); analogWrite(pinEN2, x2pwm(x)); }
  else        { digitalWrite(pinPH2, LOW);  analogWrite(pinEN2, x2pwm(-x)); }
}

void initializeBars(){
  pinMode(pinEN1, OUTPUT); pinMode(pinPH1, OUTPUT);
  pinMode(pinEN2, OUTPUT); pinMode(pinPH2, OUTPUT);
  pinMode(pinSLEEP, OUTPUT);
  digitalWrite(pinEN1, LOW); digitalWrite(pinEN2, LOW);
  digitalWrite(pinSLEEP, HIGH);
  analogWriteFrequency(pinEN1, 24000);
  analogWriteFrequency(pinEN2, 24000);
  analogWriteResolution(12);
}

// force-displacement curve (버클링 클릭) //////////////////////////////////////////
// #define F_PEAK    80
// #define F_VALLEY  50
// #define F_END     70
#define F_PEAK    100
#define F_VALLEY  70
#define F_END     90
#define Z_PEAK    0.50
#define Z_VALLEY  0.80
#define Z_END     1.0
#define CLICK_SCALE 0.5f
#define MAX_FRAME_DROPS 10

// ── F/J 랜드마크: 진동 텍스처 튜너블 ─────────────────────────────────────────────
#define ENABLE_CLICK   1        // 0: 클릭 끄고 진동만 (랜드마크 단독 비교용)
#define TEX_FREQ_HZ    100.0f    // 진동 주파수 (권장 20~60Hz)
#define TEX_AMP        0.10f    // 진동 진폭 듀티(0~1). 중심에서의 최대fffjjfjjfj
#define TEX_FORCE_HI   90.0f    // 이 힘 이상이면 진동 0 (누르는 중 → 클릭에 인계)
#define TEX_SLEW       6.0f     // 진폭 슬루레이트(1/s). 얹고 뗄 때 부드럽게
#define LM_ARM_MS      500       // 그 손에 접촉이 이만큼 지속돼야 발동(드웰). 좌/우 독립
#define LM_HOLD_MS     1000      // 손가락이 "가만히" 있은 지 이 시간이 지나면 자동 정지.
                                 // 랜드마크는 "여기가 F/J다"라는 알림이지 지속 자극이 아니다.
                                 // 움직이면(F/J 위를 어루만지면) 타이머는 그때마다 리셋되고
                                 // 이미 꺼졌더라도 다시 켜진다. F/J를 벗어나도 리셋.
#define LM_STILL_EPS   1.0f      // 정지 판정 반경(mm). 기준점에서 이만큼 벗어나면 "움직였다".
                                 // 접촉 좌표 지터보다는 크고 어루만지는 폭보다는 작게.
                                 // 너무 작으면 가만히 둬도 노이즈로 안 꺼지고,
                                 // 너무 크면 살살 문질러도 꺼진다.
// ────────────────────────────────────────────────────────────────────────────

// ── USB HID 키보드 튜너블 ──────────────────────────────────────────────────────
#define ENABLE_HID           1     // 0: HID 완전 차단(햅틱만). 촉각 튜닝 중엔 0 권장
#define HID_START_ENABLED    1     // 부팅 시 입력 활성 상태. 0이면 Serial 'h'로 켜야 함
#define HID_BOOT_LOCKOUT_MS  3000  // 부팅 후 이 시간 동안은 무조건 입력 안 함
#define HID_LOG              1     // 리포트가 바뀔 때마다 Serial로 한 줄
#define HID_MAX_KEYS         6     // 표준 HID 부트 키보드 상한. modifier는 별도
// ────────────────────────────────────────────────────────────────────────────

SideState sideL = {{{0, 0.0f, -1, -1}, {0, 0.0f, -1, -1}}};
SideState sideR = {{{0, 0.0f, -1, -1}, {0, 0.0f, -1, -1}}};

int           frameDrops = 0;
unsigned long dropTotal  = 0;

int   ctKey[SENSEL_MAX_CONTACTS];
float ctGain[SENSEL_MAX_CONTACTS];
bool  ctLeft[SENSEL_MAX_CONTACTS];
int   nContacts = 0;

int kIdxF = -1, kIdxJ = -1;

bool  lmOn[2]      = {false, false};
float lmGain[2]    = {0, 0};
float lmForce[2]   = {0, 0};
float lmX[2]       = {0, 0};           // 대표 접촉 위치(mm). 정지/이동 판정용
float lmY[2]       = {0, 0};
int   lmId[2]      = {-1, -1};
bool  lmBuckled[2] = {false, false};   // 대표 접촉이 버클링(클릭)했나
bool  lmActive[2]  = {false, false};   // 발동 게이트 통과(그 손 드웰 & 미버클 & F/J 위)

// 발동 게이트(좌/우 독립): 그 손에 손가락이 몇 개든 하나라도 얹혀 LM_ARM_MS 지속되면
// 그 손만 무장(armed). 반대 손 상태는 보지 않는다.
bool          sideOn[2]        = {false, false};  // 그 손에 접촉이 있나(직전 상태, 타이밍 시작 감지용)
unsigned long sideStartMs[2]   = {0, 0};          // 그 손 접촉이 시작된 시각
bool          armed[2]         = {false, false};  // 드웰 통과 → 그 손 랜드마크 발동 가능
bool          cleanArm[2]      = {true, true};    // 그 손 클릭 후 손가락을 다 뗐다 다시 올렸나(재무장 허용)

// 자동 정지(LM_HOLD_MS): "가만히 있은 시간"을 잰다. 기준점(stillX/Y)에서 LM_STILL_EPS
// 넘게 벗어나면 기준점을 다시 잡고 타이머도 처음부터. 벗어난 거리를 프레임 간 차이가
// 아니라 기준점 기준 누적으로 보므로 아주 천천히 문질러도 결국 이동으로 잡힌다.
bool          lmHolding[2]     = {false, false};  // 정지 타이밍을 재는 중(시작 감지용)
unsigned long lmHoldStartMs[2] = {0, 0};          // 지금 기준점에 멈춘 시각
float         stillX[2]        = {0, 0};          // 정지 기준점(mm)
float         stillY[2]        = {0, 0};
int           stillId[2]       = {-1, -1};        // 기준점을 잡은 접촉 id(바뀌면 이동 취급)
bool          lmExpired[2]     = {false, false};  // 정지 지속으로 꺼짐(움직이거나 F/J를 벗어나야 해제)

float gDriveL = 0, gDriveR = 0;

// 진동 엔벨로프(슬루된 진폭)
float texEnv[2] = {0, 0};

void classifyContacts() {
  nContacts = frame.n_contacts;
  if (nContacts > SENSEL_MAX_CONTACTS) nContacts = SENSEL_MAX_CONTACTS;
  for (int i = 0; i < nContacts; i++) {
    float x = frame.contacts[i].x_pos;
    ctKey[i]  = keyAt(x, frame.contacts[i].y_pos, &ctGain[i]);
    ctLeft[i] = keyIsLeft(ctKey[i], x);
  }
}

// 한쪽 손의 대표 접촉. 손가락 개수를 제한하지 않으므로 "그 손에서 제일 센 접촉"만
// 보면 F/J가 아닌 다른 손가락(예: 새끼)이 대표가 돼 랜드마크가 안 뜬다.
// → 타깃 키(F/J) 위의 접촉이 있으면 그중 제일 센 것을 우선 대표로 삼고,
//   없으면 그 손에서 제일 센 접촉(=lmOn false)으로 폴백한다.
int repContact(bool leftSide, int targetKey) {
  int best = -1, bestOnKey = -1; float bf = -1.0f, bfOnKey = -1.0f;
  for (int i = 0; i < nContacts; i++) {
    byte t = frame.contacts[i].type;
    if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
    if (ctLeft[i] != leftSide) continue;
    float f = frame.contacts[i].total_force;
    if (f > bf) { bf = f; best = i; }
    if (targetKey >= 0 && ctKey[i] == targetKey && f > bfOnKey) { bfOnKey = f; bestOnKey = i; }
  }
  return (bestOnKey >= 0) ? bestOnKey : best;
}

// 한쪽 손의 유효 접촉 개수
int sideCount(bool leftSide) {
  int n = 0;
  for (int i = 0; i < nContacts; i++) {
    byte t = frame.contacts[i].type;
    if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
    if (ctLeft[i] != leftSide) continue;
    n++;
  }
  return n;
}

// 그 손에서 id가 일치하는 접촉의 버클링 상태(없으면 0=미버클)
int sideButtonState(SideState* s, int id) {
  for (int k = 0; k < SIDE_SLOTS; k++)
    if (s->c[k].activeId == id) return s->c[k].buttonState;
  return 0;
}

void updateLandmarkInputs() {
  unsigned long now = millis();

  for (int s = 0; s < 2; s++) {
    bool leftSide = (s == 0);
    SideState* sp = leftSide ? &sideL : &sideR;
    int target = leftSide ? kIdxF : kIdxJ;
    int n = sideCount(leftSide);

    // 그 손에서 클릭(버클링)이 나면 그 손의 손가락을 완전히 뗄 때까지 재무장 금지.
    //  · 그 손 슬롯 중 하나라도 버클링 중이면 cleanArm=false (더러워짐)
    //  · 그 손 접촉이 0이 되면(전부 뗌) cleanArm=true (깨끗해짐)
    if (sp->c[0].buttonState || sp->c[1].buttonState) cleanArm[s] = false;
    if (n == 0)                                      cleanArm[s] = true;

    // 손가락 개수 무관: 그 손에 하나라도 얹혀 LM_ARM_MS 지속되면 그 손만 무장.
    if (n > 0) {
      if (!sideOn[s]) { sideOn[s] = true; sideStartMs[s] = now; }
      armed[s] = cleanArm[s] && (now - sideStartMs[s] >= LM_ARM_MS);
    } else {
      sideOn[s] = false;
      armed[s]  = false;
    }

    int idx = repContact(leftSide, target);
    if (idx >= 0) {
      lmGain[s]    = ctGain[idx];
      lmForce[s]   = frame.contacts[idx].total_force;
      lmX[s]       = frame.contacts[idx].x_pos;
      lmY[s]       = frame.contacts[idx].y_pos;
      lmId[s]      = frame.contacts[idx].id;
      lmOn[s]      = (ctKey[idx] == target);
      lmBuckled[s] = (sideButtonState(sp, lmId[s]) != 0);
    } else {
      lmGain[s] = 0; lmForce[s] = 0; lmId[s] = -1; lmOn[s] = false; lmBuckled[s] = false;
    }

    // 게이트(그 손만 본다): 드웰 통과 & 그 손 대표 접촉 미버클 & 대표 접촉이 F/J 위
    lmActive[s] = armed[s] && !lmBuckled[s] && lmOn[s];

    // 자동 정지: "가만히 있은" 시간이 LM_HOLD_MS를 넘으면 끈다. 움직이면 기준점과
    // 타이머를 다시 잡고 꺼진 것도 되살린다 — F/J 위를 계속 어루만지는 동안에는
    // 안 꺼지고, 손을 멈춘 채 얹어두기만 하면 1초 뒤 조용해진다.
    // (여기서 참조하는 lmActive는 이번 프레임 게이트 결과. 아래 lmExpired 반영 전이다.)
    if (!lmOn[s]) {
      lmHolding[s] = false;
      lmExpired[s] = false;
    } else if (lmActive[s]) {
      float dx = lmX[s] - stillX[s], dy = lmY[s] - stillY[s];
      bool moved = (lmId[s] != stillId[s]) ||
                   (dx * dx + dy * dy > LM_STILL_EPS * LM_STILL_EPS);
      if (!lmHolding[s] || moved) {
        lmHolding[s]     = true;
        stillX[s]        = lmX[s];
        stillY[s]        = lmY[s];
        stillId[s]       = lmId[s];
        lmHoldStartMs[s] = now;
        lmExpired[s]     = false;   // 다시 움직였으니 랜드마크 부활
      }
      if (now - lmHoldStartMs[s] >= LM_HOLD_MS) lmExpired[s] = true;
    }
    if (lmExpired[s]) lmActive[s] = false;
  }
}

// ── 진동 텍스처 출력 (매 반복 호출) ──────────────────────────────────────────────
//  tSec: 전역 위상 시간, dt: 직전 반복과의 간격
float texOutput(int s, float tSec, float dt) {
  float target = 0.0f;
  if (lmActive[s] && lmGain[s] > 0.0f) {
    float fade = 1.0f - lmForce[s] / TEX_FORCE_HI;   // 누르면 진동 죽이고 클릭에 인계
    if (fade < 0.0f) fade = 0.0f;
    target = TEX_AMP * lmGain[s] * fade;
  }
  // 진폭을 목표로 슬루 → on/off 순간의 불연속(클릭) 제거
  float step = TEX_SLEW * dt;
  float d = target - texEnv[s];
  if      (d >  step) texEnv[s] += step;
  else if (d < -step) texEnv[s] -= step;
  else                texEnv[s]  = target;

  return texEnv[s] * sinf(2.0f * PI * TEX_FREQ_HZ * tSec);
}

float forceToZ(ContactState* c, float force) {
  float z;
  if (c->buttonState == 0) {
    if (force < F_PEAK) { z = force / F_PEAK * Z_PEAK; }
    else { c->buttonState = 1; z = Z_END; }
  } else {
    if (force > F_VALLEY) {
      z = (force - F_END) * (Z_END - Z_VALLEY) / (F_END - F_VALLEY) + Z_END;
      if (z > Z_END) z = Z_END;
    } else { c->buttonState = 0; z = force / F_PEAK * Z_PEAK; }
  }
  return z;
}

void assignSlots(SideState* s, bool leftSide, int outIdx[SIDE_SLOTS]) {
  for (int k = 0; k < SIDE_SLOTS; k++) outIdx[k] = -1;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    if (s->c[k].activeId < 0) continue;
    for (int i = 0; i < nContacts; i++) {
      byte t = frame.contacts[i].type;
      if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
      if (ctLeft[i] != leftSide) continue;
      if ((int)frame.contacts[i].id == s->c[k].activeId) { outIdx[k] = i; break; }
    }
    // 담당하던 접촉이 사라짐. buttonState를 여기서 0으로 되돌리므로 릴리즈 "엣지"는
    // 사라진다. 상태 재구성 방식이라 상관없다 — heldKey만 비워두면 다음 리포트에서
    // 그 키가 빠진다.
    if (outIdx[k] < 0) {
      s->c[k].activeId = -1; s->c[k].buttonState = 0; s->c[k].prevForce = 0.0f;
      s->c[k].heldKey = -1;
    }
  }
  for (int k = 0; k < SIDE_SLOTS; k++) {
    if (outIdx[k] >= 0) continue;
    int best = -1; float bestF = 0.0f;
    for (int i = 0; i < nContacts; i++) {
      byte t = frame.contacts[i].type;
      if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
      if (ctLeft[i] != leftSide) continue;
      bool taken = false;
      for (int j = 0; j < SIDE_SLOTS; j++) if (outIdx[j] == i) { taken = true; break; }
      if (taken) continue;
      float f = frame.contacts[i].total_force;
      if (best < 0 || f > bestF) { best = i; bestF = f; }
    }
    if (best >= 0) {
      outIdx[k] = best; s->c[k].activeId = (int)frame.contacts[best].id;
      s->c[k].buttonState = 0; s->c[k].heldKey = -1;   // 새 접촉이니 이전 상태를 물려받지 않는다
    }
  }
}

float sideDrive(SideState* s, const int idx[SIDE_SLOTS], int keyOut[SIDE_SLOTS]) {
  float z = 0.0f;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    float force = 0.0f, keyGain = 0.0f; keyOut[k] = -1;
    if (idx[k] >= 0) { force = frame.contacts[idx[k]].total_force; keyOut[k] = ctKey[idx[k]]; keyGain = ctGain[idx[k]]; }

    int prevBS = s->c[k].buttonState;
    float unit = forceToZ(&s->c[k], force);

    // 버클링 전이에서 키를 래치/해제한다. 이 순간이 곧 손가락이 클릭을 느끼는 순간이고,
    // F_PEAK/F_VALLEY 히스테리시스가 그대로 디바운스 역할을 한다.
    if (!prevBS && s->c[k].buttonState)      s->c[k].heldKey = keyOut[k];  // 키다운 (-1이면 갭)
    else if (prevBS && !s->c[k].buttonState) s->c[k].heldKey = -1;         // 키업

    s->c[k].prevForce = force;
    z += CLICK_SCALE * unit * keyGain;
  }
  return z;
}

float clampDrive(float x){ if (x > 1.0f) return 1.0f; if (x < -1.0f) return -1.0f; return x; }

// ── USB HID 키보드 출력 ────────────────────────────────────────────────────────
bool hidEnabled = HID_START_ENABLED;

uint8_t hidMod  = 0,  prevMod = 0;
uint8_t hidKeys[HID_MAX_KEYS]  = {0};
uint8_t prevKeys[HID_MAX_KEYS] = {0};
bool    hidOverflow = false;

// 지금 만들고 있는 리포트에 키코드 하나를 채워 넣는다.
void hidAddCode(uint16_t code) {
  if (code == 0) return;                        // HID 출력 없는 키 (햅틱만)
  if ((code & 0xF000) == 0xE000) {              // MODIFIERKEY_* -> modifier 바이트
    hidMod |= (uint8_t)(code & 0xFF);
    return;
  }
  uint8_t u = (uint8_t)(code & 0xFF);           // KEY_* 는 0xF000|usage. 하위 8비트가 usage
  for (int i = 0; i < HID_MAX_KEYS; i++) if (hidKeys[i] == u) return;  // 같은 키 중복 방지
  for (int i = 0; i < HID_MAX_KEYS; i++) if (hidKeys[i] == 0) { hidKeys[i] = u; return; }
  hidOverflow = true;                           // 6키 초과분은 버린다 (표준 HID 한계)
}

// 5ms마다 호출. 리포트를 매번 처음부터 다시 만들고, 바뀌었을 때만 보낸다.
// 엣지를 쫓지 않으므로 릴리즈를 놓쳐 키가 고착되는 경로가 원천적으로 없다.
void updateHID(unsigned long now) {
#if ENABLE_HID
  hidMod = 0;
  for (int i = 0; i < HID_MAX_KEYS; i++) hidKeys[i] = 0;
  hidOverflow = false;

  // hidEnabled가 꺼지면 빈 리포트가 만들어지고, 그게 곧 전체 릴리즈가 된다.
  if (hidEnabled && now >= HID_BOOT_LOCKOUT_MS) {
    SideState* sides[2] = { &sideL, &sideR };
    for (int s = 0; s < 2; s++) {
      for (int k = 0; k < SIDE_SLOTS; k++) {
        ContactState* c = &sides[s]->c[k];
        if (c->buttonState && c->heldKey >= 0) hidAddCode(KEYS[c->heldKey].code);
      }
    }
  }

  bool changed = (hidMod != prevMod);
  for (int i = 0; i < HID_MAX_KEYS && !changed; i++) changed = (hidKeys[i] != prevKeys[i]);
  if (!changed) return;

  // HID_MAX_KEYS를 바꾸려면 이 6줄도 같이 손봐야 한다 (set_key1..6이 전부다).
  Keyboard.set_modifier(hidMod);
  Keyboard.set_key1(hidKeys[0]); Keyboard.set_key2(hidKeys[1]);
  Keyboard.set_key3(hidKeys[2]); Keyboard.set_key4(hidKeys[3]);
  Keyboard.set_key5(hidKeys[4]); Keyboard.set_key6(hidKeys[5]);
  Keyboard.send_now();

  prevMod = hidMod;
  for (int i = 0; i < HID_MAX_KEYS; i++) prevKeys[i] = hidKeys[i];

  #if HID_LOG
    Serial.print(">>> HID mod=0x"); Serial.print(hidMod, HEX); Serial.print(" keys=");
    bool any = false;
    for (int i = 0; i < HID_MAX_KEYS; i++) {
      if (!hidKeys[i]) continue;
      if (any) Serial.print(",");
      Serial.print(hidKeys[i], HEX);
      any = true;
    }
    if (!any) Serial.print("-");
    if (hidOverflow) Serial.print("  [OVERFLOW >6]");
    Serial.println();
  #endif
#else
  (void)now;
#endif
}

// Serial로 'h'를 받으면 입력을 켜고 끈다. 폭주할 때 가장 빠른 차단 수단.
void pollHidCommand() {
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch == 'h' || ch == 'H') {
      hidEnabled = !hidEnabled;
      Serial.print("\n>>> HID typing ");
      Serial.println(hidEnabled ? "ENABLED" : "DISABLED");
    }
  }
}

elapsedMillis loopTimer;
#define LOOP_PERIOD 5

// 이 스케치의 관심사는 키 출력이라 기본은 끈다. 200Hz로 흐르는 이 로그를 켜면
// HID_LOG의 ">>> HID" 줄이 묻힌다. 햅틱/키맵을 볼 때만 1로.
#define VERBOSE_LOG 0

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  initializeSensel();
  initializeBars();
  loopTimer = 0;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    sideL.c[k].buttonState = 0; sideL.c[k].prevForce = 0.0f; sideL.c[k].activeId = -1;
    sideR.c[k].buttonState = 0; sideR.c[k].prevForce = 0.0f; sideR.c[k].activeId = -1;
    sideL.c[k].heldKey = -1;    sideR.c[k].heldKey = -1;
  }
  frame.n_contacts = 0; nContacts = 0; frameDrops = 0; dropTotal = 0;

  for (int i = 0; i < N_KEYS; i++) {
    if (KEYS[i].label[0] == 'F' && KEYS[i].label[1] == 0) kIdxF = i;
    if (KEYS[i].label[0] == 'J' && KEYS[i].label[1] == 0) kIdxJ = i;
  }

  // 부팅 시 눌린 키가 남아 있지 않도록 빈 리포트를 한 번 보낸다.
  Keyboard.set_modifier(0);
  Keyboard.set_key1(0); Keyboard.set_key2(0); Keyboard.set_key3(0);
  Keyboard.set_key4(0); Keyboard.set_key5(0); Keyboard.set_key6(0);
  Keyboard.send_now();

#if ENABLE_HID
  Serial.print("\n=== keyboard: HID typing ");
  Serial.print(hidEnabled ? "ENABLED" : "DISABLED");
  Serial.print(" (부팅 후 "); Serial.print(HID_BOOT_LOCKOUT_MS);
  Serial.println("ms 잠금). Serial로 'h' 입력하면 토글. ===");
#else
  Serial.println("\n=== keyboard: HID 비활성 (ENABLE_HID=0). 햅틱만 동작 ===");
#endif
}

void loop() {
  // (1) 센서 읽기 + 클릭 계산: 5ms 게이트
  if (loopTimer >= LOOP_PERIOD) {
    loopTimer = 0;
    if (senselGetFrame(&frame)) { frameDrops = 0; }
    else {
      frameDrops++; dropTotal++;
      if (frameDrops >= MAX_FRAME_DROPS) frame.n_contacts = 0;
    }
    classifyContacts();

    int idxL[SIDE_SLOTS], idxR[SIDE_SLOTS];
    assignSlots(&sideL, true,  idxL);
    assignSlots(&sideR, false, idxR);

    int kiL[SIDE_SLOTS], kiR[SIDE_SLOTS];
    gDriveL = sideDrive(&sideL, idxL, kiL);
    gDriveR = sideDrive(&sideR, idxR, kiR);

    updateLandmarkInputs();

    // 키 출력. sideDrive()가 heldKey를 갱신한 뒤여야 한다.
    pollHidCommand();
    updateHID(millis());

#if VERBOSE_LOG
    Serial.print("L key=");
    Serial.print(kiL[0] >= 0 ? KEYS[kiL[0]].label : "-"); Serial.print(",");
    Serial.print(kiL[1] >= 0 ? KEYS[kiL[1]].label : "-");
    Serial.print("\ttexF="); Serial.print(lmOn[0] ? texEnv[0] : 0.0f);
    Serial.print("\t|\tR key=");
    Serial.print(kiR[0] >= 0 ? KEYS[kiR[0]].label : "-"); Serial.print(",");
    Serial.print(kiR[1] >= 0 ? KEYS[kiR[1]].label : "-");
    Serial.print("\ttexJ="); Serial.println(lmOn[1] ? texEnv[1] : 0.0f);
#endif
  }

  // (2) 바 출력: 매 반복 (사인 합성은 고속 출력이 필수)
  unsigned long nowUs = micros();
  static unsigned long lastUs = 0;
  float dt = (nowUs - lastUs) * 1e-6f;
  lastUs = nowUs;
  if (dt > 0.05f) dt = 0.05f;   // 첫 반복/스톨 보호
  float tSec = nowUs * 1e-6f;

  float lmL = texOutput(0, tSec, dt);
  float lmR = texOutput(1, tSec, dt);
#if ENABLE_CLICK
  moveBar1(clampDrive(gDriveL + lmL));
  moveBar2(clampDrive(gDriveR + lmR));
#else
  moveBar1(clampDrive(lmL));
  moveBar2(clampDrive(lmR));
#endif
}
