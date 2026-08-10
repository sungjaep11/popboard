// =============================================================================
//  vcm-position  —  keyboard/ 에서 F/J 진동 랜드마크(돌기)만 들어낸 버전
// -----------------------------------------------------------------------------
//  위치 의존은 클릭 진폭 하나에만 건다. 임계힘은 키 어디를 눌러도 F_PEAK로
//  똑같고, 엣지를 누르면 같은 힘에서 똑같이 버클링하되 클릭이 약하게 온다.
//  -> "가장자리는 흐릿한 키". 네 스케치 중 이게 기준(baseline) 조건이다.
//
//    vcm-position : 진폭만.  <- 이 파일 (= keyboard/ 마이너스 돌기)
//    vcm-force/   : 임계힘만. 엣지에서 더 세게 눌러야 넘어간다. 클릭 세기는 동일.
//    vcm-combined : 둘 다.
//    keyboard/    : 진폭 + F/J 진동 랜드마크(돌기)
//
//  진폭 곡선은 keyboard/ 와 같은 식이다:
//      gain = KEY_GAIN_MIN + (1 - KEY_GAIN_MIN) * (1 - edge)^AMP_CURVE
//  keyboard/ 는 이걸 keyAt() 안에서 만들어 돌려줬는데, 여기서는 형제 스케치들과
//  코드를 맞추려고 keyAt()은 거리만 돌려주고 ampGain()에서 곡선을 씌운다.
//  기본값(0.1 / 2.0)에서 두 곡선은 완전히 같다.
//
//  keyboard/ 와 의도적으로 다른 점 둘 (형제 스케치들과 맞춘 것):
//   1) 갭에서는 버클링하지 않는다. keyboard/ 는 키캡 밖에서도 buttonState가
//      넘어갈 수 있었고, 그러면 heldKey가 -1로 래치돼 키는 안 나가는데 손가락이
//      키캡 안으로 흔들려 들어오는 순간 바가 램프 없이 튀어나온다.
//   2) 클릭 세기를 버클링 순간에 래치한다(heldEdge). 누른 채 미끄러져도 진폭이
//      도중에 변하지 않는다. keyboard/ 는 매 프레임 다시 계산했다.
//  둘 다 되돌리고 싶으면 sideDrive()의 amp 한 줄만 고치면 된다.
//
//  ★ Tools > USB Type = "Serial + Keyboard + Mouse + Joystick" 필수.
//    (Keyboard 단독으로 하면 Serial 디버그가 죽는다. 반드시 Serial 포함본으로.)
//
//  HID 안전장치는 keyboard/ 와 동일하다:
//   - ENABLE_HID       : 컴파일 타임 차단
//   - HID_BOOT_LOCKOUT_MS : 부팅 직후 무입력 구간
//   - Serial로 'h'     : 런타임 on/off 토글
//  첫 테스트는 반드시 빈 텍스트 에디터에 포커스를 두고 할 것. 터미널/IDE 금지.
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
// keyboard/ 와 같은 값. 위치와 무관하게 키 어디서나 이 곡선 하나다.
#define F_PEAK    100
#define F_VALLEY  70
#define F_END     90
#define Z_PEAK    0.50
#define Z_VALLEY  0.80
#define Z_END     1.0
#define CLICK_SCALE 0.5f
#define MAX_FRAME_DROPS 10

// ── 위치별 클릭 세기 튜너블 ───────────────────────────────────────────────────────
// KEY_GAIN_MIN : 키캡 경계에서의 클릭 세기(중심=1.0). keyboard/ 와 같은 0.1이 기본.
//                1.0으로 두면 위치 의존이 완전히 사라진다(= 위치 무관 대조군).
// AMP_CURVE    : 감쇠 곡선의 지수. keyboard/ 와 같은 2.0이 기본. 중심 근처는
//                완만하고 가장자리에서 급격히 죽는다. 1.0이면 선형.
// EDGE_DEADBAND : 중심에서 이 안쪽은 전부 최대 세기(스위트스팟). keyboard/ 에는
//                없던 개념이라 기본 0 = keyboard/ 와 정확히 같은 곡선이다.
//                형제 스케치(vcm-force/vcm-combined)는 0.37(약 3mm)을 쓴다.
//                조건을 맞춰 비교하려면 여기도 0.37로 올려라.
#define KEY_GAIN_MIN   0.1f
#define AMP_CURVE      2.0f
#define EDGE_DEADBAND  0.0f

// 1: 버클링할 때마다 한 줄. key / 중심에서의 거리 / 그때의 클릭 세기 / 실제 힘.
#define FORCE_PROBE 1
// ────────────────────────────────────────────────────────────────────────────

// ── USB HID 키보드 튜너블 ──────────────────────────────────────────────────────
#define ENABLE_HID           1     // 0: HID 완전 차단(햅틱만). 촉각 튜닝 중엔 0 권장
#define HID_START_ENABLED    1     // 부팅 시 입력 활성 상태. 0이면 Serial 'h'로 켜야 함
#define HID_BOOT_LOCKOUT_MS  3000  // 부팅 후 이 시간 동안은 무조건 입력 안 함
#define HID_LOG              1     // 리포트가 바뀔 때마다 Serial로 한 줄
#define HID_MAX_KEYS         6     // 표준 HID 부트 키보드 상한. modifier는 별도
// ────────────────────────────────────────────────────────────────────────────

SideState sideL = {{{0, 0.0f, -1, -1, 0.0f}, {0, 0.0f, -1, -1, 0.0f}}};
SideState sideR = {{{0, 0.0f, -1, -1, 0.0f}, {0, 0.0f, -1, -1, 0.0f}}};

int           frameDrops = 0;
unsigned long dropTotal  = 0;

int   ctKey[SENSEL_MAX_CONTACTS];    // 눌린 키 인덱스 (-1: 키캡 밖)
float ctEdge[SENSEL_MAX_CONTACTS];   // 키 중심에서의 정규화 거리 (0 중심 ~ 1 경계)
bool  ctLeft[SENSEL_MAX_CONTACTS];   // true: 왼쪽 바 담당
int   nContacts = 0;

float gDriveL = 0, gDriveR = 0;

void classifyContacts() {
  nContacts = frame.n_contacts;
  if (nContacts > SENSEL_MAX_CONTACTS) nContacts = SENSEL_MAX_CONTACTS;
  for (int i = 0; i < nContacts; i++) {
    float x = frame.contacts[i].x_pos;
    ctKey[i]  = keyAt(x, frame.contacts[i].y_pos, &ctEdge[i]);
    ctLeft[i] = keyIsLeft(ctKey[i], x);
  }
}

// keyAt()의 원시 거리 -> 데드밴드를 뺀 유효 거리. 스위트스팟 안은 전부 0,
// 바깥은 데드밴드 경계에서 0, 키캡 경계에서 1이 되도록 다시 정규화한다.
float edgeNorm(float edgeRaw) {
  if (edgeRaw <= EDGE_DEADBAND) return 0.0f;
  return (edgeRaw - EDGE_DEADBAND) / (1.0f - EDGE_DEADBAND);
}

// 유효 거리 -> 클릭 진폭 배수. 중심(및 스위트스팟 안) 1.0, 키캡 경계 KEY_GAIN_MIN.
// keyboard/ 의 keyAt()이 하던 일을 그대로 가져왔다. 위치 의존은 여기 하나뿐이다.
float ampGain(float edge) {
  if (edge <= 0.0f) return 1.0f;
  if (edge > 1.0f)  edge = 1.0f;
  return KEY_GAIN_MIN + (1.0f - KEY_GAIN_MIN) * powf(1.0f - edge, AMP_CURVE);
}

// force -> 단위 버클 곡선값. 좌굴 상태는 접촉(슬롯)마다 따로 갱신된다.
// 임계힘도 변위도 위치와 무관하다 — 키 어디를 눌러도 같은 힘에서 같은 궤적으로
// 버클링한다. 위치는 호출부에서 ampGain()으로 진폭에만 곱해진다.
//   onKey: 지금 접촉이 키캡 안인가. 갭에서는 아무리 세게 눌러도 버클링하지
//          않는다. 갭에서 한 번 버클링하면 heldKey가 -1로 래치돼 키는 안 나가는데
//          바만 물려 있고, 손가락이 키캡 안으로 조금 흔들려 들어오는 순간
//          램프 없이 z가 통째로 튀어나온다. 이미 버클링한 접촉이 갭으로
//          미끄러진 경우는 막지 않는다 — 그건 heldKey가 살아 있는 정상 상태다.
//   edge : 버클링하는 순간 c->heldEdge에 래치된다. 클릭 세기를 그 값으로 고정해
//          누른 채 미끄러져도 진폭이 도중에 변하지 않게 하려는 것뿐, 힘 곡선
//          자체에는 쓰이지 않는다.
float forceToZ(ContactState* c, float force, float edge, bool onKey) {
  float z;

  if (c->buttonState == 0) {   // before buckling
    if (force < F_PEAK || !onKey) {
      z = force / F_PEAK * Z_PEAK;
      if (z > Z_PEAK) z = Z_PEAK;   // 갭에서 임계를 넘겨도 버클링 직전까지만
    }
    else {
      c->buttonState = 1;
      c->heldEdge    = edge;        // 이 세기로 뗄 때까지 간다
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
      s->c[k].heldKey = -1;  s->c[k].heldEdge = 0.0f;
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
      s->c[k].buttonState = 0; s->c[k].heldKey = -1; s->c[k].heldEdge = 0.0f;
    }
  }
}

// 한쪽 손의 슬롯 2개로 바 구동값을 계산한다.
//   keyOut[k]  : 슬롯 k가 누른 키 인덱스(없으면 -1)
//   gainOut[k] : 슬롯 k에 지금 적용 중인 클릭 세기 배수 (로그/튜닝용)
float sideDrive(SideState* s, const int idx[SIDE_SLOTS],
                int keyOut[SIDE_SLOTS], float gainOut[SIDE_SLOTS]) {
  float z = 0.0f;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    float force = 0.0f, edgeRaw = 0.0f;
    keyOut[k] = -1;
    if (idx[k] >= 0) { force = frame.contacts[idx[k]].total_force; keyOut[k] = ctKey[idx[k]]; edgeRaw = ctEdge[idx[k]]; }

    // 데드밴드를 뺀 값이 클릭 세기의 기준이 된다. 갭이면 keyAt()이 edgeRaw=1을
    // 주므로 자동으로 제일 약한 쪽이 되고, onKey=false라 어차피 진폭 0이다.
    float edge  = edgeNorm(edgeRaw);
    bool  onKey = (keyOut[k] >= 0);

    int prevBS = s->c[k].buttonState;
    // 접촉이 없어도(force=0) 호출해서 버클링 상태를 풀어줘야 한다.
    float unit = forceToZ(&s->c[k], force, edge, onKey);

    // 버클링 전이에서 키를 래치/해제한다. 이 순간이 곧 손가락이 클릭을 느끼는 순간이고,
    // F_PEAK/F_VALLEY 히스테리시스가 그대로 디바운스 역할을 한다.
    if (!prevBS && s->c[k].buttonState)      s->c[k].heldKey = keyOut[k];  // 키다운
    else if (prevBS && !s->c[k].buttonState) s->c[k].heldKey = -1;         // 키업

    // 버클링 후에는 래치된 키/위치를 따라간다 — 접촉 중심이 키 경계를 들락거려도
    // (키 사이 2.9mm 빈 띠) 바가 껐다 켜졌다 하지 않고, 누른 채 미끄러져도
    // 클릭 세기가 도중에 변하지 않는다. keyboard/ 는 매 프레임 다시 계산했다.
    // 그 동작이 필요하면 아래 두 줄을 ampGain(edge) 한 줄로 되돌리면 된다.
    bool  live  = s->c[k].buttonState ? (s->c[k].heldKey >= 0) : onKey;
    gainOut[k]  = ampGain(s->c[k].buttonState ? s->c[k].heldEdge : edge);
    float amp   = live ? gainOut[k] : 0.0f;

#if FORCE_PROBE
    if (!prevBS && s->c[k].buttonState) {
      Serial.print("[probe] key=");
      Serial.print(keyOut[k] >= 0 ? KEYS[keyOut[k]].label : "-");
      Serial.print("\tedge="); Serial.print(edgeRaw, 2);
      Serial.print("(");       Serial.print(edgeRaw * 8.1f, 1); Serial.print("mm)");
      Serial.print("\tgain="); Serial.print(gainOut[k], 2);
      Serial.print("\tth=");   Serial.print((int)F_PEAK);
      Serial.print("\tf=");    Serial.print(force, 0);
      Serial.print("\t(x");    Serial.print(force / F_PEAK, 1);
      Serial.println(" 초과)");
    }
#endif

    s->c[k].prevForce = force;
    z += CLICK_SCALE * unit * amp;
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

// 1: 매 루프 좌/우 슬롯의 키·힘·임계힘·구동값 출력.
// 임계힘(peak)이 중심에서 F_PEAK, 엣지로 갈수록 커지는지 확인할 때 켠다.
// 200Hz로 흐르므로 켜면 HID_LOG의 ">>> HID" 줄이 묻힌다.
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
    sideL.c[k].heldEdge = 0.0f; sideR.c[k].heldEdge = 0.0f;
  }
  frame.n_contacts = 0; nContacts = 0; frameDrops = 0; dropTotal = 0;

  // 부팅 시 눌린 키가 남아 있지 않도록 빈 리포트를 한 번 보낸다.
  Keyboard.set_modifier(0);
  Keyboard.set_key1(0); Keyboard.set_key2(0); Keyboard.set_key3(0);
  Keyboard.set_key4(0); Keyboard.set_key5(0); Keyboard.set_key6(0);
  Keyboard.send_now();

  Serial.print("\n=== vcm-position: 클릭 진폭 1.00 -> ");
  Serial.print(KEY_GAIN_MIN); Serial.print(" (curve ");
  Serial.print(AMP_CURVE, 1); Serial.print(", 스위트스팟 +-");
  Serial.print(EDGE_DEADBAND * 8.1f, 1); Serial.println("mm) ===");
  Serial.print("=== 임계힘 "); Serial.print((int)F_PEAK);
  Serial.println(" 고정 (위치 무관), Z 0.50->1.00 ===");
#if ENABLE_HID
  Serial.print("=== HID typing ");
  Serial.print(hidEnabled ? "ENABLED" : "DISABLED");
  Serial.print(" (부팅 후 "); Serial.print(HID_BOOT_LOCKOUT_MS);
  Serial.println("ms 잠금). Serial로 'h' 입력하면 토글. ===");
#else
  Serial.println("=== HID 비활성 (ENABLE_HID=0). 햅틱만 동작 ===");
#endif
}

void loop() {
  if (loopTimer < LOOP_PERIOD) return;
  loopTimer = 0;

  // 읽기에 실패하면 직전 프레임을 그대로 유지한다. 매번 0으로 밀면
  // 드롭 한 번에 구동값이 0으로 떨어졌다 복귀하면서 바가 튄다.
  if (senselGetFrame(&frame)) { frameDrops = 0; }
  else {
    frameDrops++; dropTotal++;
    if (frameDrops >= MAX_FRAME_DROPS) frame.n_contacts = 0;
  }
  classifyContacts();

  int idxL[SIDE_SLOTS], idxR[SIDE_SLOTS];
  assignSlots(&sideL, true,  idxL);
  assignSlots(&sideR, false, idxR);

  int   kiL[SIDE_SLOTS], kiR[SIDE_SLOTS];
  float gnL[SIDE_SLOTS], gnR[SIDE_SLOTS];
  gDriveL = sideDrive(&sideL, idxL, kiL, gnL);
  gDriveR = sideDrive(&sideR, idxR, kiR, gnR);

  // 두 바를 매 프레임 각각 갱신 -> 양손 동시 입력이 서로를 막지 않는다.
  moveBar1(clampDrive(gDriveL));
  moveBar2(clampDrive(gDriveR));

  // 키 출력. sideDrive()가 heldKey를 갱신한 뒤여야 한다.
  pollHidCommand();
  updateHID(millis());

#if VERBOSE_LOG
  Serial.print("L key=");
  Serial.print(kiL[0] >= 0 ? KEYS[kiL[0]].label : "-"); Serial.print(",");
  Serial.print(kiL[1] >= 0 ? KEYS[kiL[1]].label : "-");
  Serial.print("\tedge="); Serial.print(idxL[0] >= 0 ? ctEdge[idxL[0]] : 0.0f, 2); Serial.print(",");
                           Serial.print(idxL[1] >= 0 ? ctEdge[idxL[1]] : 0.0f, 2);
  Serial.print("\tf=");    Serial.print(sideL.c[0].prevForce, 0); Serial.print(",");
                           Serial.print(sideL.c[1].prevForce, 0);
  Serial.print("\tgain="); Serial.print(gnL[0], 2); Serial.print(",");
                           Serial.print(gnL[1], 2);
  Serial.print("\tdrive="); Serial.print(gDriveL);

  Serial.print("\t|\tR key=");
  Serial.print(kiR[0] >= 0 ? KEYS[kiR[0]].label : "-"); Serial.print(",");
  Serial.print(kiR[1] >= 0 ? KEYS[kiR[1]].label : "-");
  Serial.print("\tedge="); Serial.print(idxR[0] >= 0 ? ctEdge[idxR[0]] : 0.0f, 2); Serial.print(",");
                           Serial.print(idxR[1] >= 0 ? ctEdge[idxR[1]] : 0.0f, 2);
  Serial.print("\tf=");    Serial.print(sideR.c[0].prevForce, 0); Serial.print(",");
                           Serial.print(sideR.c[1].prevForce, 0);
  Serial.print("\tgain="); Serial.print(gnR[0], 2); Serial.print(",");
                           Serial.print(gnR[1], 2);
  Serial.print("\tdrive="); Serial.println(gDriveR);
#endif
}
