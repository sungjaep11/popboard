// =============================================================================
//  vcm-force  —  키 중심에서 멀수록 "더 세게 눌러야" 버클링되는 키보드
// -----------------------------------------------------------------------------
//  keyboard/ 에서 F/J 진동 랜드마크(돌기)를 완전히 들어내고, 대신 버클링의
//  힘-변위 곡선을 접촉 위치에 따라 바꾼다.
//
//  keyboard/ 와의 차이는 딱 하나, keyAt()이 돌려주는 위치값을 어디에 쓰느냐다.
//
//    keyboard/  : 진폭을 줄인다.  임계힘은 어디서나 F_PEAK.
//                 엣지를 누르면 같은 힘에서 똑같이 버클링하되 클릭이 약하게 온다.
//                 -> "가장자리는 흐릿하게 느껴지는 키"
//
//    vcm-force/ : 뻣뻣하게 만든다. 클릭(스냅) 크기는 어디서나 동일.
//                 엣지를 누르면 클릭 세기는 그대로인데 더 세게 눌러야 넘어가고,
//                 누르는 내내 바가 손가락을 더 세게 밀어낸다.
//                 -> "가장자리는 뻣뻣한 키". 중심을 찾아 누르게 만드는 힘 되먹임이고,
//                    빗겨 친 입력이 임계에 못 미쳐 걸러지는 효과도 같이 온다.
//
//  중심에는 EDGE_DEADBAND(약 ±3mm)의 스위트스팟이 있어서 그 안은 전부 중심과
//  똑같다. 접촉 중심은 겨냥해도 1~2mm 흔들리므로 이게 없으면 평범한 타건이
//  전부 무거워진다. 위치 의존은 그 바깥 밴드에만 걸린다.
//
//  뻣뻣함은 두 축으로 낸다 (자세한 설명은 아래 튜너블 주석):
//    1) 임계힘   F_PEAK/F_VALLEY/F_END 에 forceScale(edge) 배수
//    2) 프리로드 Z_PEAK/Z_VALLEY/Z_END 를 Z_EDGE_PRELOAD*edge 만큼 통째로 상승
//               (지금은 0 = 끔. 중심 Z_END가 듀티 상한을 쓰고 있어서다)
//  (2)에서 세 Z에 같은 값을 더하므로 스냅 크기 Z_END-Z_PEAK는 위치와 무관하게
//  일정하다. 즉 "무른 키 / 뻣뻣한 키"이지 "약한 클릭 / 센 클릭"이 아니다.
//  (1)만으로는 타이핑 속도에서 차이가 거의 안 느껴진다 — 임계를 넘는 시점만
//  1~2 프레임 당겨질 뿐, 손가락이 받는 힘은 똑같기 때문이다.
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
// 아래 세 힘은 "키 중심(edge=0)에서의" 값이다. 실제 임계는 forceScale()을 곱한 것.
#define F_PEAK    100
#define F_VALLEY  70
#define F_END     90
// Z_* 도 "중심에서의" 값이다. 엣지에서는 Z_EDGE_PRELOAD만큼 통째로 올라간다.
// 지금은 원래 keyboard/ 와 같은 값 = 중심의 클릭 감각은 건드리지 않는다.
#define Z_PEAK    0.50
#define Z_VALLEY  0.80
#define Z_END     1.0
#define CLICK_SCALE 0.5f
#define MAX_FRAME_DROPS 10

// ── 위치별 뻣뻣함 튜너블 ─────────────────────────────────────────────────────────
// 위치 의존을 두 갈래로 건다. 임계힘만 올리면 "언제 넘어가는가"만 바뀌는데,
// 타이핑처럼 10~20ms만에 힘이 치솟는 입력에서는 그 시점 차이가 1~2 프레임으로
// 뭉개져서 손가락이 못 느낀다. 실제로 느껴지는 건 손가락을 밀어내는 힘 자체라
// 변위(Z)도 같이 올려야 한다.
//
// EDGE_DEADBAND : 스위트스팟. 중심에서 이 안쪽은 전부 "중심"으로 쳐서 임계 F_PEAK
//                그대로다. keyAt()의 edge는 키 반높이(8.1mm)로 정규화된 값이므로
//                0.37 = 약 3mm. 이 구간이 반드시 있어야 한다 — 손가락 접촉 중심은
//                겨냥해서 쳐도 1~2mm는 흔들리기 때문에, 데드밴드 없이 곡선만
//                가파르게 하면 "정확히 기하학적 중심을 때린 순간"만 가벼워지고
//                평범한 타건이 전부 무거워진다. 실제 키캡에 평평한 면이 있는 것과
//                같은 이치다. 데드밴드 바깥에서만 0->1로 다시 정규화된다.
// F_EDGE_SCALE : 키캡 경계(edge=1)에서의 임계힘 배수. 3.0 = 엣지에서 F_PEAK 300.
//                데드밴드 덕분에 이 3배가 바깥 밴드에만 몰려서 대비가 커진다.
//                너무 키우면 가장자리가 아예 안 눌리는 죽은 영역이 된다.
// F_EDGE_CURVE : 데드밴드 바깥에서 배수 곡선의 지수. 1.0 = 선형(기본).
//                0.5 이하는 쓰지 마라. 데드밴드 경계에서 기울기가 무한대라
//                스위트스팟 테두리가 벽처럼 느껴진다.
// Z_EDGE_PRELOAD : 엣지에서 힘-변위 곡선 전체를 들어올리는 양(듀티).
//                Z_PEAK/Z_VALLEY/Z_END에 똑같이 더해지므로 스냅 크기
//                (= Z_END - Z_PEAK)는 위치와 무관하게 일정하다. 즉 클릭 "세기"는
//                안 건드리면서 엣지 키가 손가락을 더 세게 밀어내게 만든다.
//                지금은 0 = 끔. 중심 Z_END가 이미 듀티 상한 1.0을 쓰고 있어서
//                엣지를 더 세게 만들 여유가 없기 때문이다. 임계힘 대비만으로
//                부족하면 아래 한 세트로 바꿔라 (중심이 그만큼 물러진다):
//                   Z_PEAK 0.40 / Z_VALLEY 0.70 / Z_END 0.90 / PRELOAD 0.10
#define EDGE_DEADBAND  0.37f
#define F_EDGE_SCALE   3.0f
#define F_EDGE_CURVE   1.0f
#define Z_EDGE_PRELOAD 0.0f

// 1: 버클링할 때마다 한 줄. key / edge / 임계힘 / 임계를 넘은 그 프레임의 실제 힘.
//    맨 오른쪽 f가 임계보다 훨씬 크면(예: 임계 100인데 f=380) 한 프레임 만에
//    임계를 몇 배로 뛰어넘었다는 뜻이고, 그 상태로는 임계를 어떻게 조절해도
//    차이가 안 느껴진다. 그때는 F_PEAK 자체를 그 힘 근처로 올려야 한다.
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

// 유효 거리 -> 임계힘 배수. 스위트스팟 안 1.0, 키캡 경계 F_EDGE_SCALE.
float forceScale(float edge) {
  if (edge <= 0.0f) return 1.0f;
  if (edge > 1.0f)  edge = 1.0f;   // keyAt()이 1을 넘길 일은 없지만 방어
  return 1.0f + (F_EDGE_SCALE - 1.0f) * powf(edge, F_EDGE_CURVE);
}

// force -> 단위 버클 곡선값. 좌굴 상태는 접촉(슬롯)마다 따로 갱신된다.
//   edge: 지금 접촉의 키 중심으로부터의 거리(0~1). 버클링 전에는 매 프레임
//         반영되고, 버클링하는 순간 c->heldEdge에 래치돼 릴리즈까지 그걸 쓴다.
// 임계힘(F_*)에는 배수를, 변위(Z_*)에는 프리로드를 건다. 스냅 크기는 Z_PEAK와
// Z_END에 같은 값이 더해지므로 위치와 무관하게 항상 (Z_END - Z_PEAK)로 일정하다.
//   onKey: 지금 접촉이 키캡 안인가. 갭에서는 아무리 세게 눌러도 버클링하지
//          않는다. 갭에서 한 번 버클링하면 heldKey가 -1로 래치돼 키는 안 나가는데
//          바만 물려 있고, 손가락이 키캡 안으로 조금 흔들려 들어오는 순간
//          램프 없이 z가 통째로 튀어나온다. 이미 버클링한 접촉이 갭으로
//          미끄러진 경우는 막지 않는다 — 그건 heldKey가 살아 있는 정상 상태다.
float forceToZ(ContactState* c, float force, float edge, bool onKey) {
  float e  = (c->buttonState == 0) ? edge : c->heldEdge;
  float fs = forceScale(e);
  float zp = Z_PEAK   + Z_EDGE_PRELOAD * e;
  float zv = Z_VALLEY + Z_EDGE_PRELOAD * e;
  float ze = Z_END    + Z_EDGE_PRELOAD * e;
  float z;

  if (c->buttonState == 0) {   // before buckling
    float fPeak = F_PEAK * fs;
    if (force < fPeak || !onKey) {
      z = force / fPeak * zp;
      if (z > zp) z = zp;      // 갭에서 임계를 넘겨도 버클링 직전까지만
    }
    else {
      c->buttonState = 1;
      c->heldEdge    = edge;   // 이 뻣뻣함으로 뗄 때까지 간다
      z = ze;
    }
  }
  else {                       // after buckling
    float fValley = F_VALLEY * fs;
    float fEnd    = F_END * fs;
    if (force > fValley) {
      z = (force - fEnd) * (ze - zv) / (fEnd - fValley) + ze;
      if (z > ze) z = ze;
    }
    else {
      c->buttonState = 0;
      // 풀린 뒤엔 다시 현재 위치 기준
      z = force / (F_PEAK * forceScale(edge)) * (Z_PEAK + Z_EDGE_PRELOAD * edge);
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
//   peakOut[k] : 슬롯 k에 지금 적용 중인 버클링 임계힘 (로그/튜닝용)
float sideDrive(SideState* s, const int idx[SIDE_SLOTS],
                int keyOut[SIDE_SLOTS], float peakOut[SIDE_SLOTS]) {
  float z = 0.0f;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    float force = 0.0f, edgeRaw = 0.0f;
    keyOut[k] = -1;
    if (idx[k] >= 0) { force = frame.contacts[idx[k]].total_force; keyOut[k] = ctKey[idx[k]]; edgeRaw = ctEdge[idx[k]]; }

    // 데드밴드를 뺀 값이 아래 전부(임계 배수, 프리로드, 래치)의 기준이 된다.
    // 갭이면 keyAt()이 edgeRaw=1을 주므로 자동으로 제일 뻣뻣한 쪽이 된다.
    float edge  = edgeNorm(edgeRaw);
    bool  onKey = (keyOut[k] >= 0);

    int prevBS = s->c[k].buttonState;
    // 접촉이 없어도(force=0) 호출해서 버클링 상태를 풀어줘야 한다.
    float unit = forceToZ(&s->c[k], force, edge, onKey);

    // 버클링 후에는 래치된 위치가 진짜 임계를 정한다.
    peakOut[k] = F_PEAK * forceScale(s->c[k].buttonState ? s->c[k].heldEdge : edge);

    // 버클링 전이에서 키를 래치/해제한다. 이 순간이 곧 손가락이 클릭을 느끼는 순간이고,
    // F_PEAK/F_VALLEY 히스테리시스가 그대로 디바운스 역할을 한다.
    if (!prevBS && s->c[k].buttonState)      s->c[k].heldKey = keyOut[k];  // 키다운
    else if (prevBS && !s->c[k].buttonState) s->c[k].heldKey = -1;         // 키업

    // 햅틱 진폭은 0 아니면 1. 버클링 후에는 래치된 키를 따라간다 — 접촉 중심이
    // 키 경계를 들락거려도(키 사이 2.9mm 빈 띠) 바가 껐다 켜졌다 하지 않는다.
    float amp = (s->c[k].buttonState ? (s->c[k].heldKey >= 0) : onKey) ? 1.0f : 0.0f;

#if FORCE_PROBE
    if (!prevBS && s->c[k].buttonState) {
      Serial.print("[probe] key=");
      Serial.print(keyOut[k] >= 0 ? KEYS[keyOut[k]].label : "-");
      Serial.print("\tedge="); Serial.print(edgeRaw, 2);
      Serial.print("(");       Serial.print(edgeRaw * 8.1f, 1); Serial.print("mm)");
      Serial.print("\teff=");  Serial.print(edge, 2);
      Serial.print("\tth=");   Serial.print(peakOut[k], 0);
      Serial.print("\tf=");    Serial.print(force, 0);
      Serial.print("\t(x");    Serial.print(force / peakOut[k], 1);
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

  Serial.print("\n=== vcm-force: 임계힘 "); Serial.print((int)F_PEAK);
  Serial.print(" (중심) ~ "); Serial.print((int)(F_PEAK * F_EDGE_SCALE));
  Serial.print(" (엣지), curve="); Serial.print(F_EDGE_CURVE);
  Serial.print(", 스위트스팟 +-"); Serial.print(EDGE_DEADBAND * 8.1f, 1); Serial.print("mm");
  Serial.print(", preload="); Serial.print(Z_EDGE_PRELOAD);
  Serial.print(" (Z "); Serial.print(Z_PEAK); Serial.print("->");
  Serial.print(Z_END); Serial.print(" 중심, ");
  Serial.print(Z_PEAK + Z_EDGE_PRELOAD); Serial.print("->");
  Serial.print(Z_END + Z_EDGE_PRELOAD); Serial.println(" 엣지) ===");
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
  float pkL[SIDE_SLOTS], pkR[SIDE_SLOTS];
  gDriveL = sideDrive(&sideL, idxL, kiL, pkL);
  gDriveR = sideDrive(&sideR, idxR, kiR, pkR);

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
  Serial.print("\tpeak="); Serial.print(pkL[0], 0); Serial.print(",");
                           Serial.print(pkL[1], 0);
  Serial.print("\tdrive="); Serial.print(gDriveL);

  Serial.print("\t|\tR key=");
  Serial.print(kiR[0] >= 0 ? KEYS[kiR[0]].label : "-"); Serial.print(",");
  Serial.print(kiR[1] >= 0 ? KEYS[kiR[1]].label : "-");
  Serial.print("\tedge="); Serial.print(idxR[0] >= 0 ? ctEdge[idxR[0]] : 0.0f, 2); Serial.print(",");
                           Serial.print(idxR[1] >= 0 ? ctEdge[idxR[1]] : 0.0f, 2);
  Serial.print("\tf=");    Serial.print(sideR.c[0].prevForce, 0); Serial.print(",");
                           Serial.print(sideR.c[1].prevForce, 0);
  Serial.print("\tpeak="); Serial.print(pkR[0], 0); Serial.print(",");
                           Serial.print(pkR[1], 0);
  Serial.print("\tdrive="); Serial.println(gDriveR);
#endif
}
