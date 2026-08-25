// =============================================================================
//  vcm-tune  —  vcm-position / vcm-force / vcm-combined 을 하나로 합친 튜닝 펌웨어
// -----------------------------------------------------------------------------
//  세 조건의 차이는 "키 중심에서의 거리(edge)"를 무엇에 쓰느냐 하나뿐이다.
//
//    mode 0 = equal    : 아무 데도 안 쓴다. 키 위 어디를 눌러도 같은 클릭, 같은
//                        임계힘. 위치 의존이 없는 베이스라인이다.
//    mode 1 = position : 클릭 진폭에만.  임계힘은 어디서나 F_PEAK.
//    mode 2 = force    : 임계힘에만.     클릭 세기는 어디서나 동일.
//    mode 3 = beta     : 클릭은 mode 0과 똑같이 두고(어디서나 같은 힘, 같은 세기)
//                        그 위에 진동을 얹어서 편심을 알린다.
//
//  그래서 진폭 축(ampGain)과 힘 축(forceScale)을 각각 켜고 끌 수 있게 만들면
//  한 펌웨어가 세 조건을 전부 낸다. mode를 바꾸면 그 자리에서 조건이 바뀌므로
//  피험자를 세워두고 스케치를 다시 업로드할 필요가 없다.
//
//  ── mode 3 (beta) ────────────────────────────────────────────────────────────
//  mode 1과 2의 실패를 같은 방식으로 피한다: 에러 신호를 성공 신호와 다른 채널에
//  싣는다. 임계힘도 클릭 세기도 mode 0과 동일해서 키는 어디를 눌러도 똑같이,
//  반드시 눌린다. 위치는 오직 진동 세기로만 나간다.
//
//    vibWhen 0 = 누를 때   버클링 순간 + vibDelay 뒤에 vibDur 길이의 버스트
//            1 = 뗄 때     힘이 fValley 아래로 떨어져 풀리는 순간에 같은 버스트
//            2 = 누를 때+뗄 때  위 둘을 다. 누르고 있는 동안은 조용하다 — 지속음은
//                          두지 않는다. 손가락이 멈춰 있는 동안 계속 흔들면 수용기가
//                          몇 백 ms 만에 적응해 세기 단서가 사라지고, 그 사이 다른
//                          키가 눌리면 두 진동이 섞여서 어느 쪽 편심인지 못 가른다.
//
//  vibCenter / vibEdge 가 중심과 키캡 경계에서의 세기고(0~1000), 그 사이는
//  vibCurve 지수로 보간한다. vibCenter=0, vibEdge=400 이면 "중심은 조용하고
//  빗겨 칠수록 웅웅거린다"가 된다. 둘을 뒤집으면 반대(중심에서만 울리는 랜드마크)다.
//
//  ★ vibDelay 가 기본 50ms인 이유: 버클링 스텝은 광대역 트랜지언트라 그 직후
//    20~50ms는 순방향 마스킹으로 진동이 묻힌다. 0으로 두면 클릭에 먹혀서
//    아무리 세게 줘도 안 느껴진다. vibWhen 1(뗄 때)은 경쟁 트랜지언트가 없으니
//    0으로 둬도 된다.
//  ★ vibFreq 기본 220Hz도 같은 이유다 — 클릭(저역 스텝)과 스펙트럼을 벌리고,
//    누르는 중 손가락 임피던스가 높아 변위가 잘 안 나는 상황에서도 파치니
//    소체가 잡아내는 대역이다. 무하중 텍스처용인 100Hz와는 다르다.
//
//    다만 이 바는 가동부 질량이 있어서 공진 위로 가면 같은 힘에 변위가 1/w^2로
//    죽는다. keyboard/ 의 TEX_FREQ_HZ 주석에 적힌 "권장 20~60Hz"가 그 실측
//    경계이므로, 220이 실제로 이 바에서 나오는지는 'vib <세기> 0' 으로 계속
//    울리게 해놓고 vibFreq를 훑어서 직접 확인해 볼 것.
//
//  ── 진동이 안 느껴질 때 ──────────────────────────────────────────────────────
//    1) 'vib 800 0' : 접촉·모드와 무관하게 양쪽 바를 계속 흔든다.
//       안 느껴지면 vibFreq가 이 바의 기계적 대역 밖이다. 20~60을 훑어라.
//       느껴지면 액추에이터는 멀쩡하니 아래로 간다.
//    2) mode 가 3인가. 0~2 에서는 vibAmpFor()가 통째로 0을 돌려준다.
//    3) vibCenter 가 기본 0이다. 키 중심을 정확히 누르면 설계상 조용하다.
//       #probe 줄의 vib= 값이 그 타건에서 실제로 나간 세기다. 0이면 그 이유다.
//    4) vibDelay 를 0으로 내려 봐라. 클릭에 묻히는 건지 애초에 안 나오는 건지
//       갈린다 (묻히는 거면 진동이 "지저분한 클릭"으로는 느껴진다).
//
//  ★ mode 0도 바는 정상적으로 돈다 — 손끝에 클릭이 그대로 잡힌다. 다른 조건과
//    다른 점은 그 클릭이 키 어디를 누르든 똑같다는 것뿐이다. 그래서 "햅틱이
//    있고 없고"가 아니라 "햅틱이 위치를 알려주고 아니고"를 가르는 대조군이 된다.
//
//  세 스케치의 #define 튜너블은 전부 런타임 변수(P.*)가 됐다. Serial 한 줄로
//  바꾸고 즉시 손끝에서 확인한다. test.html의 튜너 패널이 이 프로토콜을 쓴다.
//
//  ★ 원본 세 스케치는 그대로 남아 있다. 값을 확정하면 여기서 뽑은 숫자를
//    해당 스케치의 #define에 옮겨 적고 그걸 실험에 쓰면 된다.
//
//  ── Serial 프로토콜 (줄 단위, 115200) ────────────────────────────────────────
//    set <name> <value>   값 하나 변경        -> "#ok <name>=<v>"
//    get                  전체 덤프           -> "#cfg name=v name=v ..."
//    defaults <0|1|2>     그 모드의 기본값을 통째로 적용
//    save / load          EEPROM 저장 / 복원
//    hid <0|1> / h        HID 타이핑 on/off (h는 토글)
//    ping                 -> "#pong"
//
//  ★ Arduino 시리얼 모니터로 직접 칠 때는 줄바꿈("Newline")을 켤 것.
//    옛 버전은 'h' 한 글자를 즉시 먹었지만 지금은 줄이 끝나야 처리한다.
//
//  기계가 읽는 줄은 전부 '#'으로 시작한다 (#cfg #ok #err #probe #rel #tel #hid).
//  사람이 읽는 배너/경고는 '#' 없이 나간다.
//
//  ★ Tools > USB Type = "Serial + Keyboard + Mouse + Joystick" 필수.
//
//  HID 안전장치는 형제 스케치와 동일하다. 첫 테스트는 반드시 빈 텍스트
//  에디터에 포커스를 두고 할 것. 터미널/IDE 금지.
// =============================================================================

#include <USBHost_t36.h>
#include <EEPROM.h>

#if !defined(KEYBOARD_INTERFACE)
  #error "Tools > USB Type 을 'Serial + Keyboard + Mouse + Joystick' 으로 설정하세요."
#endif

#include "sensel.h"
#include "sensel_register_map.h"
#include "keymap.h"
#include "barstate.h"
#include "params.h"

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

// ── 런타임 튜너블 ──────────────────────────────────────────────────────────────
// 원본 스케치의 #define과 1:1로 대응한다. 의미는 그쪽 주석 그대로다.
//
//   fPeak/fValley/fEnd : 버클링 힘-변위 곡선의 세 힘. "키 중심에서의" 값이고
//                        실제 임계는 forceScale()을 곱한 것 (mode 1/2에서만).
//   zPeak/zValley/zEnd : 같은 곡선의 변위(듀티). 위치와 무관하다.
//   deadband           : 스위트스팟. keyAt()의 edge는 키 반높이 8.5mm로 정규화된
//                        값이라 0.35 = 약 3mm. 이 안쪽은 전부 "중심"으로 친다.
//   fEdgeScale/Curve   : 키캡 경계에서의 임계힘 배수와 그 곡선의 지수 (힘 축).
//                        지수는 ampCurve와 같은 방향이다 — 크게 잡을수록
//                        스위트스팟 바로 바깥에서 급격히 무거워진다.
//   gainMin/ampCurve   : 키캡 경계에서의 클릭 세기와 감쇠 지수 (진폭 축).
//   calOX/calOY        : keymap.h의 센서 원점 보정. 오버레이가 밀렸을 때 쓴다.
// 구조체 정의는 params.h에 있다 (Arduino 자동 프로토타입 때문에 헤더여야 한다).
#define P_MAGIC 0x56434D37UL   // 'VCM7' — 구조가 바뀌면 이 값을 올릴 것
                               //  (VCM6 = mode 3(beta)과 vib* 가 없던 판. Params에
                               //   필드가 늘었으므로 옛 EEPROM을 그대로 읽으면 안 된다)
                               //  (VCM5 = mode 0이 '햅틱 없음'이고 mode 3에 combined가
                               //   있던 판. 0의 뜻이 바뀌고 3이 사라졌으므로 옛 EEPROM을
                               //   그대로 읽으면 엉뚱한 조건으로 돈다)
                               //  (VCM4 = mode 0이 position이던 판)
                               //  (VCM3 = zPreload가 있던 판)

Params P;

// 기본값 한 벌. 곡선(F_*/Z_*)은 세 조건이 공유하고, mode 는 그 곡선을 위치에 따라
// 어떻게 비틀지만 정한다. mode 0(equal)은 아무 것도 안 비튼다.
void applyDefaults(int mode) {
  P.magic      = P_MAGIC;
  P.mode       = mode;
  P.fPeak      = 100;   P.fValley = 70;   P.fEnd = 90;
  P.zPeak      = 0.50f; P.zValley = 0.80f; P.zEnd = 1.00f;
  P.clickScale = 0.5f;
  P.fEdgeScale = 3.0f;  P.fEdgeCurve = 1.0f;
  P.gainMin    = 0.1f;  P.ampCurve   = 2.0f;
  P.deadband   = 0.0f;
  // mode 3. 중심 0 = 똑바로 치면 조용하고 빗겨 칠수록 울린다 (교정 피드백 방향).
  P.vibWhen    = 0;     P.vibCenter = 0.0f;  P.vibEdge = 400.0f;  P.vibCurve = 1.0f;
  P.vibFreq    = 220.0f; P.vibDur   = 60;    P.vibDelay = 50;
  P.calOX      = -18.8f; P.calOY = 23.0f;
  P.loopPeriod = 5;     P.maxFrameDrops = 10;
  P.hid        = 1;     P.probe = 1;  P.verbose = 0;  P.hidLog = 1;
  P.telHz      = 30;
}

// mode 0(equal)은 두 축 모두 꺼진 상태다 — forceScale()과 ampGain()이 나란히
// 1.0을 돌려주므로 키 위 어디서나 같은 임계, 같은 세기가 된다.
inline bool useForceAxis() { return P.mode == 2; }
inline bool useAmpAxis()   { return P.mode == 1; }
// mode 3은 두 축 모두 꺼진 채(= mode 0과 같은 클릭) 진동 축만 켠다.
inline bool useVibAxis()   { return P.mode == 3; }

// 부팅 후 이 시간 동안은 무조건 입력하지 않는다. 런타임에 못 바꾼다 —
// 잠금 시간을 런타임에 줄일 수 있으면 잠금이 아니다.
#define HID_BOOT_LOCKOUT_MS  3000
#define HID_MAX_KEYS         6     // 표준 HID 부트 키보드 상한. modifier는 별도

// ── 튜너블 테이블 ──────────────────────────────────────────────────────────────
// set/get이 이 표 하나만 본다. 파라미터를 늘리려면 여기 한 줄만 추가하면 된다.
//
// ★ 여기 lo/hi는 조용히 "자른다"(거절하지 않는다). 그래서 UI 쪽 입력 범위가 이 표보다
//   넓으면, 화면에는 넣은 값이 남고 기기는 잘린 값으로 도는 어긋남이 생긴다. 로그는
//   화면 값을 적으므로 나중에 실제와 다른 조건이 기록된다. test.html의 PARAMS와
//   범위를 맞춰둘 것 — 특히 nmin/nmax(숫자칸 자유 범위)가 있는 것들:
//   fEdgeScale·fEdgeCurve·ampCurve·vibCurve 는 숫자칸과 같은 0.01~100 을 받는다.
//   여기가 좁으면 UI에는 넣은 값이 남고 EEPROM에는 잘린 값이 들어가서, 재부팅
//   뒤에 "저장이 안 됐다"로 보인다. 지수는 powf()에 그대로 들어갈 뿐이라
//   넓혀도 안전하다.
static const Tunable TUNABLES[] = {
  {"mode",        nullptr, &P.mode,         0,     3},
  {"fPeak",       &P.fPeak,       nullptr,  1,  2000},
  {"fValley",     &P.fValley,     nullptr,  0,  2000},
  {"fEnd",        &P.fEnd,        nullptr,  1,  2000},
  {"zPeak",       &P.zPeak,       nullptr,  0,     1},
  {"zValley",     &P.zValley,     nullptr,  0,     1},
  {"zEnd",        &P.zEnd,        nullptr,  0,     1},
  {"clickScale",  &P.clickScale,  nullptr,  0,     1},
  {"deadband",    &P.deadband,    nullptr,  0,  0.95f},
  {"fEdgeScale",  &P.fEdgeScale,  nullptr,  1,   100},
  {"fEdgeCurve",  &P.fEdgeCurve,  nullptr, 0.01f, 100},
  {"gainMin",     &P.gainMin,     nullptr,  0,     1},
  {"ampCurve",    &P.ampCurve,    nullptr, 0.01f, 100},
  {"vibWhen",     nullptr, &P.vibWhen,      0,     2},
  {"vibCenter",   &P.vibCenter,   nullptr,  0,  1000},
  {"vibEdge",     &P.vibEdge,     nullptr,  0,  1000},
  {"vibCurve",    &P.vibCurve,    nullptr, 0.01f, 100},
  {"vibFreq",     &P.vibFreq,     nullptr,  10,  500},
  {"vibDur",      nullptr, &P.vibDur,       5,   500},
  {"vibDelay",    nullptr, &P.vibDelay,     0,   300},
  {"calOX",       &P.calOX,       nullptr, -60,   60},
  {"calOY",       &P.calOY,       nullptr, -60,   60},
  {"loopPeriod",  nullptr, &P.loopPeriod,   1,    50},
  {"maxDrops",    nullptr, &P.maxFrameDrops,1,   200},
  {"hid",         nullptr, &P.hid,          0,     1},
  {"probe",       nullptr, &P.probe,        0,     1},
  {"verbose",     nullptr, &P.verbose,      0,     1},
  {"hidLog",      nullptr, &P.hidLog,       0,     1},
  {"telHz",       nullptr, &P.telHz,        0,   100},
};
static const int N_TUNABLES = sizeof(TUNABLES)/sizeof(TUNABLES[0]);

SideState sideL = {{{0, 0.0f, -1, -1, 0.0f, 0.0f, 0, 0.0f},
                    {0, 0.0f, -1, -1, 0.0f, 0.0f, 0, 0.0f}}};
SideState sideR = {{{0, 0.0f, -1, -1, 0.0f, 0.0f, 0, 0.0f},
                    {0, 0.0f, -1, -1, 0.0f, 0.0f, 0, 0.0f}}};

int           frameDrops = 0;
unsigned long dropTotal  = 0;

int   ctKey[SENSEL_MAX_CONTACTS];    // 눌린 키 인덱스 (-1: 키캡 밖)
float ctEdge[SENSEL_MAX_CONTACTS];   // 키 중심에서의 정규화 거리 (0 중심 ~ 1 경계)
bool  ctLeft[SENSEL_MAX_CONTACTS];   // true: 왼쪽 바 담당
int   nContacts = 0;

float gDriveL = 0, gDriveR = 0;

SlotInfo infoL[SIDE_SLOTS], infoR[SIDE_SLOTS];   // 정의는 params.h

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
  if (edgeRaw <= P.deadband) return 0.0f;
  if (P.deadband >= 0.999f)  return 0.0f;
  return (edgeRaw - P.deadband) / (1.0f - P.deadband);
}

// 유효 거리 -> 임계힘 배수. 힘 축이 꺼진 모드(position)에서는 항상 1.0이다.
float forceScale(float edge) {
  if (!useForceAxis() || edge <= 0.0f) return 1.0f;
  if (edge > 1.0f) edge = 1.0f;
  return 1.0f + (P.fEdgeScale - 1.0f) * (1.0f - powf(1.0f - edge, P.fEdgeCurve));
  // return 1.0f + (P.fEdgeScale - 1.0f) * (powf(edge, P.fEdgeCurve));
}

// 유효 거리 -> 클릭 진폭 배수. 진폭 축이 꺼진 모드(force)에서는 항상 1.0이다.
float ampGain(float edge) {
  if (!useAmpAxis() || edge <= 0.0f) return 1.0f;
  if (edge > 1.0f) edge = 1.0f;
  return P.gainMin + (1.0f - P.gainMin) * powf(1.0f - edge, P.ampCurve);
}

// 유효 거리 -> 진동 진폭(듀티 0~1). 진동 축이 꺼진 모드에서는 항상 0이다.
// 슬라이더 눈금은 0~1000이고 1000 = 듀티 1.0 이다. 클릭이 이미 clickScale(기본 0.5)
// 만큼 쓰고 있으므로 500을 넘기면 클릭과 겹치는 구간에서 clampDrive()에 잘린다.
// 잘린다고 조용해지지는 않는다 — 사인의 위쪽이 깎여 파형이 거칠어질 뿐이다.
//
// 곡선은 forceScale()과 같은 꼴이라 vibCurve의 의미도 fEdgeCurve와 같다:
// 지수를 키울수록 스위트스팟 바로 바깥에서 급격히 세지고 가장자리 쪽은 평평해진다.
// vibEdge < vibCenter로 뒤집어 넣어도 그대로 동작한다 (중심에서만 울리는 랜드마크).
float vibAmpFor(float edge) {
  if (!useVibAxis()) return 0.0f;
  if (edge < 0.0f) edge = 0.0f;
  if (edge > 1.0f) edge = 1.0f;
  float v = P.vibCenter + (P.vibEdge - P.vibCenter) * (1.0f - powf(1.0f - edge, P.vibCurve));
  if (v < 0.0f) v = 0.0f;
  return v * 0.001f;
}

// force -> 단위 버클 곡선값. 좌굴 상태는 접촉(슬롯)마다 따로 갱신된다.
//   edge : 버클링 전에는 매 프레임 반영되고, 버클링하는 순간 c->heldEdge에
//          래치돼 릴리즈까지 그걸 쓴다. 누른 채 미끄러져도 임계와 세기가
//          도중에 변하지 않게 하려는 것이다.
//   onKey: 지금 접촉이 키캡 안인가. 갭에서는 아무리 세게 눌러도 버클링하지
//          않는다. 갭에서 버클링하면 heldKey가 -1로 래치돼 키는 안 나가는데
//          바만 물려 있고, 손가락이 키캡 안으로 흔들려 들어오는 순간 램프 없이
//          z가 통째로 튀어나온다.
float forceToZ(ContactState* c, float force, float edge, bool onKey) {
  float e  = (c->buttonState == 0) ? edge : c->heldEdge;
  float fs = forceScale(e);
  float zp = P.zPeak, zv = P.zValley, ze = P.zEnd;   // 변위는 위치와 무관하다
  float z;

  if (c->buttonState == 0) {   // before buckling
    float fPeak = P.fPeak * fs;
    if (force < fPeak || !onKey) {
      z = force / fPeak * zp;
      if (z > zp) z = zp;      // 갭에서 임계를 넘겨도 버클링 직전까지만
    }
    else {
      c->buttonState = 1;
      c->heldEdge    = edge;   // 이 뻣뻣함/세기로 뗄 때까지 간다
      c->maxForce    = force;  // 여기서부터 릴리즈까지의 최대값을 쌓는다
      z = ze;
    }
  }
  else {                       // after buckling
    float fValley = P.fValley * fs;
    float fEnd    = P.fEnd * fs;
    float den     = fEnd - fValley;
    if (den < 1e-3f) den = 1e-3f;   // fValley >= fEnd로 잘못 넣어도 죽지 않게
    if (force > fValley) {
      z = (force - fEnd) * (ze - zv) / den + ze;
      if (z > ze) z = ze;
    }
    else {
      c->buttonState = 0;
      // 풀린 뒤엔 다시 현재 위치 기준
      z = force / (P.fPeak * forceScale(edge)) * P.zPeak;
    }
  }

  return z;
}

// 버클링했던 접촉이 풀릴 때 한 줄. #probe(키다운)와 짝이 되며, 그 사이에
// 이 손가락이 준 최대 힘을 싣는다. 릴리즈 경로가 둘(힘이 F_VALLEY 아래로
// 내려간 경우 / 접촉이 통째로 사라진 경우)이라 양쪽에서 부른다.
void probeRelease(ContactState* c) {
  if (!P.probe || c->heldKey < 0) return;
  Serial.print("#rel key=");
  Serial.print(KEYS[c->heldKey].label);
  Serial.print(" fmax="); Serial.print(c->maxForce, 0);
  Serial.print(" th=");   Serial.println(P.fPeak * forceScale(c->heldEdge), 0);
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
      probeRelease(&s->c[k]);            // 손가락을 통째로 뗀 경우가 여기다
      s->c[k].activeId = -1; s->c[k].buttonState = 0; s->c[k].prevForce = 0.0f;
      s->c[k].heldKey = -1;  s->c[k].heldEdge = 0.0f;  s->c[k].maxForce = 0.0f;
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

// 한쪽 손의 슬롯 2개로 바 구동값을 계산한다. out[]에 이번 프레임 상태를 남긴다.
float sideDrive(SideState* s, const int idx[SIDE_SLOTS], SlotInfo out[SIDE_SLOTS]) {
  float z = 0.0f;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    float force = 0.0f, edgeRaw = 0.0f;
    int   key   = -1;
    if (idx[k] >= 0) {
      force   = frame.contacts[idx[k]].total_force;
      key     = ctKey[idx[k]];
      edgeRaw = ctEdge[idx[k]];
    }

    // 데드밴드를 뺀 값이 아래 전부(임계 배수, 진폭, 프리로드, 래치)의 기준이 된다.
    // 갭이면 keyAt()이 edgeRaw=1을 주므로 자동으로 제일 뻣뻣하고 제일 약한 쪽이 된다.
    float edge  = edgeNorm(edgeRaw);
    bool  onKey = (key >= 0);

    int prevBS = s->c[k].buttonState;
    // 누르고 있는 동안 최대힘을 쌓는다. forceToZ()가 상태를 바꾸기 전에 봐야
    // 릴리즈 직전 프레임의 힘까지 들어간다.
    if (prevBS && force > s->c[k].maxForce) s->c[k].maxForce = force;
    // 접촉이 없어도(force=0) 호출해서 버클링 상태를 풀어줘야 한다.
    float unit = forceToZ(&s->c[k], force, edge, onKey);
    if (prevBS && !s->c[k].buttonState) {   // 힘이 빠져 풀린 경우
      probeRelease(&s->c[k]);               // heldKey는 아래에서 비워지므로 지금 찍는다
      s->c[k].maxForce = 0.0f;
    }

    // 버클링 후에는 래치된 위치가 진짜 임계와 세기를 정한다.
    float eEff = s->c[k].buttonState ? s->c[k].heldEdge : edge;

    // 버클링 전이에서 키를 래치/해제한다. 이 순간이 곧 손가락이 클릭을 느끼는 순간이고,
    // fPeak/fValley 히스테리시스가 그대로 디바운스 역할을 한다.
    bool down = (!prevBS &&  s->c[k].buttonState);   // 키다운
    bool up   = ( prevBS && !s->c[k].buttonState);   // 키업
    // 진동 버스트 발사. 세기는 양쪽 다 heldEdge(버클링 순간의 편심)로 정한다 —
    // 누를 때와 뗄 때가 같은 값을 써야 "같은 타건에 대한 같은 피드백"이 된다.
    // vibWhen 2는 누를 때와 뗄 때 둘 다 — 같은 코드로 두 번 발사할 뿐이라,
    // 눌러진 채 계속 나가는 성분은 어느 모드에도 없다. 눌렀다 vibDelay 안에
    // 떼면 아래 대입이 진행 중인 다운 버스트를 릴리즈 버스트로 갈아치운다
    // (한 슬롯이 버스트 하나만 갖는다). 세기가 같으니 손끝에서는 그냥
    // "한 번 울렸다"가 된다.
    // 릴리즈 버스트는 "힘이 fValley 아래로 떨어져 풀린" 경로에서만 나간다.
    // 접촉이 통째로 사라진 경로(assignSlots)에서는 안 나간다 — 손가락이 이미
    // 센서를 떠났으니 바를 흔들어 봐야 아무도 못 느낀다. 타이핑에서는 힘이
    // 먼저 fValley를 지나므로 실질적으로 항상 이쪽으로 걸린다.
    if (useVibAxis() && ((P.vibWhen != 1 && down) || (P.vibWhen != 0 && up))) {
      unsigned long nowUs = micros();
      s->c[k].vibT0  = nowUs ? nowUs : 1;            // 0은 "비활성" 표시라 피한다
      s->c[k].vibAmp = vibAmpFor(s->c[k].heldEdge);
    }
    if (down)     s->c[k].heldKey = key;
    else if (up)  s->c[k].heldKey = -1;

    // 버클링 후에는 래치된 키를 따라간다 — 접촉 중심이 키 경계를 들락거려도
    // (키 사이 2.9mm 빈 띠) 바가 껐다 켜졌다 하지 않는다.
    bool  live = s->c[k].buttonState ? (s->c[k].heldKey >= 0) : onKey;
    float gain = ampGain(eEff);   // mode 0에서는 위치와 무관하게 1.0
    float amp  = live ? gain : 0.0f;

    out[k].key     = key;
    out[k].edgeRaw = edgeRaw;
    out[k].force   = force;
    out[k].peak    = P.fPeak * forceScale(eEff);
    out[k].gain    = gain;
    out[k].vib     = vibAmpFor(eEff);

    if (P.probe && !prevBS && s->c[k].buttonState) {
      Serial.print("#probe key=");
      Serial.print(key >= 0 ? KEYS[key].label : "-");
      Serial.print(" edge="); Serial.print(edgeRaw, 2);
      Serial.print(" mm=");   Serial.print(edgeRaw * 8.5f, 1);
      Serial.print(" eff=");  Serial.print(edge, 2);
      Serial.print(" th=");   Serial.print(out[k].peak, 0);
      Serial.print(" gain="); Serial.print(gain, 2);
      Serial.print(" vib=");  Serial.print(out[k].vib, 3);
      Serial.print(" f=");    Serial.print(force, 0);
      Serial.print(" x=");    Serial.println(force / out[k].peak, 1);
    }

    s->c[k].prevForce = force;
    z += P.clickScale * unit * amp;
  }
  return z;
}

// ── mode 3 진동 합성 ──────────────────────────────────────────────────────────
// ★ 이 아래 둘은 loopPeriod 게이트 밖에서, 매 루프 반복마다 불려야 한다.
//   기본 loopPeriod 5ms = 200Hz 인데 vibFreq 는 그 위(기본 220Hz)라, 게이트
//   안에서 부르면 에일리어싱으로 전혀 다른 주파수가 나온다.

// 슬롯 하나가 지금 내야 할 진동 "진폭"(부호 없는 봉투). 사인은 여기서 곱하지
// 않는다 — 같은 바에 물린 두 슬롯이 각자 위상을 갖고 더해지면 서로 간섭해서
// 세기가 들쭉날쭉해지므로, 진폭만 더한 뒤 바깥에서 사인 하나를 곱한다.
float vibSlotAmp(ContactState* c, unsigned long nowUs) {
  if (!useVibAxis()) { c->vibT0 = 0; return 0.0f; }

  // 세 vibWhen 전부 이벤트 버스트다. 눌러져 있는 동안 나가는 성분은 없다.
  if (!c->vibT0) return 0.0f;
  long t   = (long)(nowUs - c->vibT0) - (long)P.vibDelay * 1000L;
  long dur = (long)P.vibDur * 1000L;
  if (t < 0)    return 0.0f;                 // 아직 지연 구간
  if (t >= dur) { c->vibT0 = 0; return 0.0f; }
  // 봉투를 sin(pi*p)로 잡아 양 끝이 정확히 0이 되게 한다. 사각 봉투로 하면
  // 시작과 끝의 불연속이 각각 또 하나의 클릭으로 느껴져서 "진동 한 번"이 아니라
  // "딱-웅-딱"이 된다.
  return c->vibAmp * sinf(PI * (float)t / (float)dur);
}

// 한쪽 바의 진동 출력. 위상은 손별로 하나만 누적시킨다.
//   ★ 위상을 micros()에서 매번 다시 만들지 않는 이유: float 가수가 24비트라
//     micros()가 커지면 t*1e-6 의 분해능이 주기보다 굵어져 주파수가 흔들린다.
//     dt를 적분하면 그 문제가 없다.
float vibSide(SideState* s, float* phase, unsigned long nowUs, float dt, float testAmp) {
  float amp = testAmp;
  for (int k = 0; k < SIDE_SLOTS; k++) amp += vibSlotAmp(&s->c[k], nowUs);
  if (amp <= 0.0f) { *phase = 0.0f; return 0.0f; }   // 조용할 땐 위상도 리셋
  *phase += 2.0f * PI * P.vibFreq * dt;
  if (*phase > 2.0f * PI) *phase -= 2.0f * PI;
  return amp * sinf(*phase);
}

// ── 진동 테스트 ('vib' 명령) ─────────────────────────────────────────────────
// 접촉도 버클링도 mode도 보지 않고 양쪽 바를 그냥 흔든다. "진동이 안 느껴진다"가
// 액추에이터/주파수 문제인지 트리거가 안 걸리는 문제인지를 가르는 게 목적이다.
// 이게 느껴지는데 타건에서 안 느껴지면 트리거·세기·마스킹 쪽이고,
// 이것조차 안 느껴지면 vibFreq가 이 바의 기계적 대역 밖이다.
unsigned long vibTestT0  = 0;      // 0 = 꺼짐
unsigned long vibTestDur = 0;      // us. 0 = 끌 때까지 계속
float         vibTestAmp = 0.0f;

// ★ 매 루프 한 번만 불러야 한다 (좌/우에서 각각 부르면 첫 호출이 상태를 끄고
//   두 번째는 0을 받는다). loop()에서 한 번 계산해 양쪽에 나눠준다.
float vibTestNow(unsigned long nowUs) {
  if (!vibTestT0) return 0.0f;
  long t = (long)(nowUs - vibTestT0);
  if (vibTestDur == 0) return vibTestAmp;                 // 지속 — 봉투 없음
  if (t >= (long)vibTestDur) { vibTestT0 = 0; return 0.0f; }
  return vibTestAmp * sinf(PI * (float)t / (float)vibTestDur);
}

float vibPhaseL = 0.0f, vibPhaseR = 0.0f;
float gVibL = 0.0f, gVibR = 0.0f;   // 텔레메트리용 마지막 값

float clampDrive(float x){ if (x > 1.0f) return 1.0f; if (x < -1.0f) return -1.0f; return x; }

// mode나 곡선을 바꾸면 물려 있던 버클링 상태가 새 파라미터와 안 맞는다.
// (예: 임계를 올린 순간 이미 버클링한 접촉이 릴리즈 임계 위에 떠 버린다)
// 전부 풀고 다시 쌓게 한다. 손가락이 올라가 있어도 다음 프레임에 복구된다.
void resetBuckling() {
  SideState* sides[2] = { &sideL, &sideR };
  for (int s = 0; s < 2; s++)
    for (int k = 0; k < SIDE_SLOTS; k++) {
      sides[s]->c[k].buttonState = 0;
      sides[s]->c[k].heldKey     = -1;
      sides[s]->c[k].heldEdge    = 0.0f;
      sides[s]->c[k].maxForce    = 0.0f;
      sides[s]->c[k].vibT0       = 0;
      sides[s]->c[k].vibAmp      = 0.0f;
    }
  vibPhaseL = vibPhaseR = 0.0f;
  gVibL = gVibR = 0.0f;
  vibTestT0 = 0; vibTestAmp = 0.0f;
  moveBar1(0); moveBar2(0);
}

// ── USB HID 키보드 출력 ────────────────────────────────────────────────────────
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

// 매 루프 호출. 리포트를 매번 처음부터 다시 만들고, 바뀌었을 때만 보낸다.
// 엣지를 쫓지 않으므로 릴리즈를 놓쳐 키가 고착되는 경로가 원천적으로 없다.
void updateHID(unsigned long now) {
  hidMod = 0;
  for (int i = 0; i < HID_MAX_KEYS; i++) hidKeys[i] = 0;
  hidOverflow = false;

  // P.hid가 꺼지면 빈 리포트가 만들어지고, 그게 곧 전체 릴리즈가 된다.
  if (P.hid && now >= HID_BOOT_LOCKOUT_MS) {
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

  if (P.hidLog) {
    Serial.print("#hid mod=0x"); Serial.print(hidMod, HEX); Serial.print(" keys=");
    bool any = false;
    for (int i = 0; i < HID_MAX_KEYS; i++) {
      if (!hidKeys[i]) continue;
      if (any) Serial.print(",");
      Serial.print(hidKeys[i], HEX);
      any = true;
    }
    if (!any) Serial.print("-");
    if (hidOverflow) Serial.print(" overflow=1");
    Serial.println();
  }
}

void sendKeyboardRelease() {
  Keyboard.set_modifier(0);
  Keyboard.set_key1(0); Keyboard.set_key2(0); Keyboard.set_key3(0);
  Keyboard.set_key4(0); Keyboard.set_key5(0); Keyboard.set_key6(0);
  Keyboard.send_now();
  prevMod = 0;
  for (int i = 0; i < HID_MAX_KEYS; i++) prevKeys[i] = 0;
}

// ── Serial 명령 ────────────────────────────────────────────────────────────────
char cmdBuf[64];
int  cmdLen = 0;

void printOne(const Tunable* t) {
  Serial.print(t->name); Serial.print('=');
  if (t->f) Serial.print(*t->f, 4); else Serial.print(*t->i);
}

void printCfg() {
  Serial.print("#cfg");
  for (int i = 0; i < N_TUNABLES; i++) { Serial.print(' '); printOne(&TUNABLES[i]); }
  Serial.println();
}

// 대소문자 무시 비교. strcasecmp는 툴체인마다 있고 없고가 갈려서 직접 쓴다.
bool ieq(const char* a, const char* b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++; b++;
  }
  return *a == *b;
}

const Tunable* findTunable(const char* name) {
  for (int i = 0; i < N_TUNABLES; i++)
    if (ieq(name, TUNABLES[i].name)) return &TUNABLES[i];
  return nullptr;
}

// 값 하나 적용. 범위 밖은 자른다(거절하지 않는다) — 슬라이더를 끝까지 끌었을 때
// 아무 반응 없이 거절되는 것보다 상한에 붙는 편이 손끝으로 확인하기 낫다.
bool setParam(const char* name, float v) {
  const Tunable* t = findTunable(name);
  if (!t) return false;
  if (v < t->lo) v = t->lo;
  if (v > t->hi) v = t->hi;

  bool structural = (t->i == &P.mode);           // 곡선의 의미 자체가 바뀌는 것
  if (t->f) *t->f = v; else *t->i = (int)lroundf(v);

  if (t->f == &P.calOX) CAL_OX = P.calOX;        // keymap.h의 전역과 동기
  if (t->f == &P.calOY) CAL_OY = P.calOY;
  if (t->i == &P.hid && !P.hid) sendKeyboardRelease();   // 끄는 즉시 전체 릴리즈
  if (structural) resetBuckling();

  Serial.print("#ok "); printOne(t); Serial.println();
  return true;
}

void loadFromEeprom(bool quiet) {
  Params tmp;
  EEPROM.get(0, tmp);
  if (tmp.magic != P_MAGIC) {
    if (!quiet) Serial.println("#err eeprom empty");
    return;
  }
  P = tmp;
  CAL_OX = P.calOX; CAL_OY = P.calOY;
  resetBuckling();
  if (!quiet) { Serial.println("#ok loaded"); printCfg(); }
}

void handleCommand(char* line) {
  while (*line == ' ') line++;
  if (!*line) return;

  char* cmd = strtok(line, " \t");
  if (!cmd) return;
  char* a1  = strtok(nullptr, " \t");
  char* a2  = strtok(nullptr, " \t");

  if (ieq(cmd, "set") || ieq(cmd, "s")) {
    if (!a1 || !a2) { Serial.println("#err usage: set <name> <value>"); return; }
    if (!setParam(a1, atof(a2))) { Serial.print("#err unknown param "); Serial.println(a1); }
  }
  else if (ieq(cmd, "get") || ieq(cmd, "g")) {
    printCfg();
  }
  else if (ieq(cmd, "defaults") || ieq(cmd, "d")) {
    int m = a1 ? atoi(a1) : P.mode;
    if (m < 0 || m > 3) { Serial.println("#err mode 0..3"); return; }
    int keepHid = P.hid, keepTel = P.telHz;
    applyDefaults(m);
    P.hid = keepHid; P.telHz = keepTel;   // 안전장치와 UI 연결은 건드리지 않는다
    CAL_OX = P.calOX; CAL_OY = P.calOY;
    resetBuckling();
    printCfg();
  }
  else if (ieq(cmd, "save")) {
    P.magic = P_MAGIC;
    EEPROM.put(0, P);
    Serial.println("#ok saved");
  }
  else if (ieq(cmd, "load")) {
    loadFromEeprom(false);
  }
  else if (ieq(cmd, "hid")) {
    if (a1) setParam("hid", atof(a1));
    else    setParam("hid", P.hid ? 0 : 1);
  }
  else if (ieq(cmd, "h")) {              // 옛 스케치와 같은 토글
    setParam("hid", P.hid ? 0 : 1);
  }
  else if (ieq(cmd, "vib") || ieq(cmd, "v")) {
    // vib            -> vibEdge 세기로 vibDur 만큼 한 번
    // vib 700        -> 700/1000 세기로 vibDur 만큼 한 번
    // vib 700 0      -> 700/1000 으로 끌 때까지 계속 (주파수 스윕용)
    // vib 0          -> 정지
    float a  = a1 ? atof(a1) : P.vibEdge;
    long  ms = a2 ? atol(a2) : (long)P.vibDur;
    if (a < 0) a = 0;
    if (a > 1000) a = 1000;
    if (a <= 0) {
      vibTestT0 = 0; vibTestAmp = 0.0f;
      Serial.println("#ok vibtest off");
    } else {
      unsigned long now = micros();
      vibTestT0  = now ? now : 1;
      vibTestDur = (ms > 0) ? (unsigned long)ms * 1000UL : 0UL;
      vibTestAmp = a * 0.001f;
      Serial.print("#ok vibtest amp="); Serial.print((int)a);
      Serial.print(" freq=");           Serial.print(P.vibFreq, 0);
      Serial.print(" ms=");             Serial.print(ms);
      Serial.println(ms > 0 ? "" : " (계속 — 'vib 0' 으로 정지)");
    }
  }
  else if (ieq(cmd, "ping")) {
    Serial.println("#pong vcm-tune");
  }
  else {
    Serial.print("#err unknown cmd "); Serial.println(cmd);
  }
}

void pollSerial() {
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (cmdLen) { cmdBuf[cmdLen] = 0; handleCommand(cmdBuf); cmdLen = 0; }
    } else if (cmdLen < (int)sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = (char)ch;
    } else {
      cmdLen = 0;                                 // 너무 긴 줄은 통째로 버린다
      Serial.println("#err line too long");
    }
  }
}

// ── 텔레메트리 ─────────────────────────────────────────────────────────────────
// 한쪽당 힘이 큰 슬롯 하나만 보낸다. UI가 지금 어느 키를 얼마로 누르고 있는지
// 보여주는 용도라 두 슬롯을 다 보낼 이유가 없다.
elapsedMillis telTimer;

void printSlot(char tag, const SlotInfo in[SIDE_SLOTS]) {
  int b = (in[1].force > in[0].force) ? 1 : 0;
  Serial.print(' '); Serial.print(tag); Serial.print('=');
  Serial.print(in[b].key);                  Serial.print(',');
  Serial.print(in[b].edgeRaw, 2);           Serial.print(',');
  Serial.print(in[b].force, 0);             Serial.print(',');
  Serial.print(in[b].peak, 0);              Serial.print(',');
  Serial.print(in[b].gain, 2);   Serial.print(',');
  Serial.print(in[b].vib, 3);
}

void sendTelemetry() {
  Serial.print("#tel");
  printSlot('l', infoL);
  printSlot('r', infoR);
  Serial.print(" dl="); Serial.print(gDriveL, 3);
  Serial.print(" dr="); Serial.print(gDriveR, 3);
  Serial.print(" vl="); Serial.print(gVibL, 3);
  Serial.print(" vr="); Serial.print(gVibR, 3);
  Serial.print(" n=");  Serial.print(nContacts);
  Serial.print(" drop="); Serial.println(dropTotal);
}

elapsedMillis loopTimer;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  applyDefaults(0);            // 부팅 기본은 equal (위치 의존 없는 베이스라인)
  initializeSensel();
  initializeBars();
  // EEPROM 복원은 반드시 initializeBars() 뒤에. loadFromEeprom()이 resetBuckling()
  // 으로 바를 0으로 미는데, 핀 설정 전에 그걸 하면 엉뚱한 듀티가 나간다.
  loadFromEeprom(true);        // 저장된 게 있으면 그걸로 덮는다
  CAL_OX = P.calOX; CAL_OY = P.calOY;

  loopTimer = 0; telTimer = 0;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    sideL.c[k].buttonState = 0; sideL.c[k].prevForce = 0.0f; sideL.c[k].activeId = -1;
    sideR.c[k].buttonState = 0; sideR.c[k].prevForce = 0.0f; sideR.c[k].activeId = -1;
    sideL.c[k].heldKey = -1;    sideR.c[k].heldKey = -1;
    sideL.c[k].heldEdge = 0.0f; sideR.c[k].heldEdge = 0.0f;
    sideL.c[k].maxForce = 0.0f; sideR.c[k].maxForce = 0.0f;
  }
  frame.n_contacts = 0; nContacts = 0; frameDrops = 0; dropTotal = 0;

  // 부팅 시 눌린 키가 남아 있지 않도록 빈 리포트를 한 번 보낸다.
  sendKeyboardRelease();

  Serial.println();
  Serial.println("=== vcm-tune: mode 0=equal 1=position 2=force 3=beta(vibration) ===");
  Serial.print("=== HID typing ");
  Serial.print(P.hid ? "ENABLED" : "DISABLED");
  Serial.print(" (부팅 후 "); Serial.print(HID_BOOT_LOCKOUT_MS);
  Serial.println("ms 잠금). 'h' + Enter 로 토글. ===");
  Serial.println("=== 명령: set <name> <v> / get / defaults <0|1|2|3> / save / load ===");
  Serial.println("===       vib [0~1000] [ms]  진동 테스트 (ms=0 이면 계속, 'vib 0' 정지) ===");
  printCfg();
}

// loop()는 두 층으로 나뉜다.
//   (1) loopPeriod(기본 5ms) 게이트 : 센서 읽기 · 클릭 계산 · HID · 로그
//   (2) 매 반복                      : 진동 합성 + 바 출력
// (2)를 게이트 안에 넣으면 200Hz로 샘플링한 220Hz 사인이 되어 mode 3이 전혀
// 다른 주파수로 나온다. 클릭 값(gDriveL/R)은 게이트 주기로만 갱신되지만 그건
// 원래도 그랬고, 그 위에 얹히는 진동만 고속으로 돌면 된다.
void loop() {
  if (loopTimer >= (unsigned long)P.loopPeriod) {
    loopTimer = 0;
    frameStep();
  }

  // (2) 매 반복: 진동은 여기서만 만들어진다.
  unsigned long nowUs = micros();
  static unsigned long lastUs = 0;
  float dt = (nowUs - lastUs) * 1e-6f;
  lastUs = nowUs;
  if (dt > 0.05f) dt = 0.05f;      // 첫 반복 / 스톨 보호

  float testAmp = vibTestNow(nowUs);   // 매 루프 한 번만
  gVibL = vibSide(&sideL, &vibPhaseL, nowUs, dt, testAmp);
  gVibR = vibSide(&sideR, &vibPhaseR, nowUs, dt, testAmp);

  // 두 바를 각각 갱신 -> 양손 동시 입력이 서로를 막지 않는다.
  moveBar1(clampDrive(gDriveL + gVibL));
  moveBar2(clampDrive(gDriveR + gVibR));
}

// 센서 한 프레임 처리. loopPeriod 주기로만 불린다.
void frameStep() {
  // 읽기에 실패하면 직전 프레임을 그대로 유지한다. 매번 0으로 밀면
  // 드롭 한 번에 구동값이 0으로 떨어졌다 복귀하면서 바가 튄다.
  if (senselGetFrame(&frame)) { frameDrops = 0; }
  else {
    frameDrops++; dropTotal++;
    if (frameDrops >= P.maxFrameDrops) frame.n_contacts = 0;
  }
  classifyContacts();

  int idxL[SIDE_SLOTS], idxR[SIDE_SLOTS];
  assignSlots(&sideL, true,  idxL);
  assignSlots(&sideR, false, idxR);

  gDriveL = sideDrive(&sideL, idxL, infoL);
  gDriveR = sideDrive(&sideR, idxR, infoR);
  // 바 출력은 여기서 하지 않는다 — loop()의 (2)층이 진동까지 얹어서 내보낸다.

  // 키 출력. sideDrive()가 heldKey를 갱신한 뒤여야 한다.
  pollSerial();
  updateHID(millis());

  if (P.telHz > 0 && telTimer >= (unsigned long)(1000 / P.telHz)) {
    telTimer = 0;
    sendTelemetry();
  }

  if (P.verbose) {
    Serial.print("#v l=");
    Serial.print(infoL[0].key >= 0 ? KEYS[infoL[0].key].label : "-"); Serial.print(",");
    Serial.print(infoL[1].key >= 0 ? KEYS[infoL[1].key].label : "-");
    Serial.print(" edge="); Serial.print(infoL[0].edgeRaw, 2); Serial.print(",");
                            Serial.print(infoL[1].edgeRaw, 2);
    Serial.print(" f=");    Serial.print(infoL[0].force, 0); Serial.print(",");
                            Serial.print(infoL[1].force, 0);
    Serial.print(" th=");   Serial.print(infoL[0].peak, 0); Serial.print(",");
                            Serial.print(infoL[1].peak, 0);
    Serial.print(" drive="); Serial.print(gDriveL, 3);

    Serial.print(" | r=");
    Serial.print(infoR[0].key >= 0 ? KEYS[infoR[0].key].label : "-"); Serial.print(",");
    Serial.print(infoR[1].key >= 0 ? KEYS[infoR[1].key].label : "-");
    Serial.print(" edge="); Serial.print(infoR[0].edgeRaw, 2); Serial.print(",");
                            Serial.print(infoR[1].edgeRaw, 2);
    Serial.print(" f=");    Serial.print(infoR[0].force, 0); Serial.print(",");
                            Serial.print(infoR[1].force, 0);
    Serial.print(" th=");   Serial.print(infoR[0].peak, 0); Serial.print(",");
                            Serial.print(infoR[1].peak, 0);
    Serial.print(" drive="); Serial.println(gDriveR, 3);
  }
}
