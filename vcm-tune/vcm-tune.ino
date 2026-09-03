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
//            2 = 누를 때+뗄 때  위 둘을 다. 셋 다 이벤트 버스트고, 누르고 있는
//                          동안은 조용하다.
//            3 = 상시      버클링을 아예 안 본다. 접촉이 움직이는 동안 현재 편심을
//                          계속 내보내고, 전부 vibDur 동안 멈추면 꺼진다. 다시
//                          움직이면 즉시 재개한다. 세기는 항상 실시간 위치를 따라간다 — 눌러서
//                          버클링한 뒤에도, 누른 채로 손가락을 중심에 다시 맞추면
//                          진동이 그만큼 잦아든다 (0~2 는 발사 순간 값에 고정).
//
//  ★ 0~2 와 3 은 목적이 다르다. 섞어 읽지 말 것.
//    0~2 는 "방금 친 그 타건이 얼마나 빗나갔나"를 사후에 알려주는 피드백이다.
//    3 은 진짜 키보드의 키캡 모서리처럼 "치기 전에 손끝으로 더듬어 위치를 찾는"
//    탐색용이다 — 실제 키보드에서 손가락을 올려놓고 홈 포지션을 확인하는 그 동작.
//
//    그래서 3 에는 0~2 를 만들 때 지속음을 배제했던 이유가 그대로 남아 있다:
//    손가락이 멈춰 있으면 수용기가 몇 백 ms 만에 적응해 세기 단서가 흐려지고,
//    같은 손의 두 접촉이 겹치면 진동이 더해져 어느 쪽 편심인지 못 가른다
//    (같은 주파수의 슬롯 진폭은 그냥 더한다). 3 은 손가락이 '움직이는 동안'
//    쓰는 모드라고 보는 게 맞다 — 정지 상태의 절대 세기를 읽히려는 용도로는
//    여전히 0~2 가 맞다.
//
//  vibCenter / vibEdge 가 데드밴드 끝점과 키캡 경계에서의 세기고(0~1000), 그
//  사이는 vibCurve 지수로 보간한다. vibCenter=0, vibEdge=400 이면 "중심은 조용하고
//  빗겨 칠수록 웅웅거린다"가 된다. 둘을 뒤집으면 반대(중심 쪽에서만 울리는 랜드마크)다.
//
//  ★ 기준점이 키 중심이 아니라 '데드밴드 끝점'이다. 데드밴드 안(스위트스팟)은
//    vibCenter 값과 무관하게 무음이고, 경계를 넘는 순간 vibCenter 로 뛴다.
//    그 계단이 "여기서부터 빗나갔다"는 문턱으로 손끝에 잡히는 게 목적이다.
//    deadband 가 0(기본값)이면 조용한 구역이 없어 계단도 없다.
//
//  현재 KEYS[]에 등록된 키캡 사이 틈은 vibGap/vibGapFreq로 울린다.
//  미매핑 키 자리와 키보드 외곽은 상시(vibWhen 3)에서도 무음이다.
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
//    defaults <0|1|2|3>   그 모드의 기본값을 통째로 적용
//    save / load          EEPROM 저장 / 복원
//    hid <0|1> / h        HID 타이핑 on/off (h는 토글)
//    ping                 -> "#pong"
//
//  ★ Arduino 시리얼 모니터로 직접 칠 때는 줄바꿈("Newline")을 켤 것.
//    옛 버전은 'h' 한 글자를 즉시 먹었지만 지금은 줄이 끝나야 처리한다.
//
//  기계가 읽는 줄은 전부 '#'으로 시작한다 (#cfg #ok #err #probe #rel #miss #tel #hid).
//
//  #probe / #rel 은 "버클링한" 타건의 짝이다. 버클링에 못 닿은 누름은 #miss 로
//  따로 나간다 — 갭(키 사이 빈 띠)을 눌렀거나, 키캡 위인데 그 자리 임계에 못
//  미친 경우다. mode 2(vcm-force)는 엣지 임계를 올려서 빗겨 친 입력을 일부러
//  거르는 조건이라, 그렇게 걸러진 입력을 세지 않으면 조건 평가가 반쪽이 된다.
//  문턱은 missMin (기본 30). 0으로 두면 아예 안 뱉는다.
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
#define P_MAGIC 0x56434D3CUL   // VCM12 — 구조/저장 의미가 바뀌면 이 값을 올릴 것
                               //  (VCM11 -> 12: Params 끝에 vibMove 추가)
                               //  (VCM10 -> 11: 상시 모드의 vibDur를 정지 타이머로 사용)
                               //  (VCM9 = vibGap 이 없던 판. Params에 필드가 늘었다)
                               //  (VCM8 = vibGapFreq 가 없던 판. Params에 필드가 늘었다)
                               //  (VCM7 = missMin 이 없던 판. Params에 필드가 늘었다)
                               //  (VCM6 = mode 3(beta)과 vib* 가 없던 판. Params에
                               //   필드가 늘었으므로 옛 EEPROM을 그대로 읽으면 안 된다)
                               //  (VCM5 = mode 0이 '햅틱 없음'이고 mode 3에 combined가
                               //   있던 판. 0의 뜻이 바뀌고 3이 사라졌으므로 옛 EEPROM을
                               //   그대로 읽으면 엉뚱한 조건으로 돈다)
                               //  (VCM4 = mode 0이 position이던 판)
                               //  (VCM3 = zPreload가 있던 판)

Params P;

// 기본값 한 벌. test.html의 defaultsFor()와 반드시 같이 바꾼다.
// 2026-08-26 force-8 실험에 사용한 튜닝값을 기본으로 고정했다.
// 곡선(F_*/Z_*)은 세 조건이 공유하고, mode 는 그 곡선을 위치에 따라
// 어떻게 비틀지만 정한다. mode 0(equal)은 아무 것도 안 비튼다.
void applyDefaults(int mode) {
  P.magic      = P_MAGIC;
  P.mode       = mode;
  P.fPeak      = 90;    P.fValley = 50;   P.fEnd = 70;
  P.zPeak      = 0.50f; P.zValley = 0.80f; P.zEnd = 1.00f;
  P.clickScale = 0.5f;
  P.fEdgeScale = 5.0f;  P.fEdgeCurve = 1.0f;
  P.gainMin    = 0.3f;  P.ampCurve   = 2.0f;
  P.deadband   = 0.47f;
  // mode 3. 중심 0 = 똑바로 치면 조용하고 빗겨 칠수록 울린다 (교정 피드백 방향).
  P.vibWhen    = 0;     P.vibCenter = 0.0f;  P.vibEdge = 400.0f;  P.vibGap = 50.0f;
  P.vibCurve   = 1.0f;
  P.vibFreq    = 200.0f; P.vibGapFreq = 120.0f;
  P.vibDur     = 50;     P.vibDelay = 0;  // 상시를 선택하면 setParam()이 2000으로 전환
  P.calOX      = -18.8f; P.calOY = 23.0f;
  P.loopPeriod = 5;     P.maxFrameDrops = 10;
  P.hid        = 1;     P.probe = 1;  P.verbose = 0;  P.hidLog = 1;
  // 30 = 살짝 스치는 접촉과 "누르려고 한" 것을 가르는 선. 타건은 보통 200 넘게
  // 가므로 넉넉히 아래고, 손을 얹어만 둔 손가락(대개 10~20)은 안 걸린다.
  P.missMin    = 30.0f;
  P.telHz      = 30;
  P.vibMove    = 5.0f;   // 기존 고정값. HTML에서 0.5~1mm로 낮춰 슬라이딩 반응 튜닝
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
  {"vibWhen",     nullptr, &P.vibWhen,      0,     3},
  {"vibCenter",   &P.vibCenter,   nullptr,  0,  1000},
  {"vibEdge",     &P.vibEdge,     nullptr,  0,  1000},
  {"vibGap",      &P.vibGap,      nullptr,  0,  1000},
  {"vibCurve",    &P.vibCurve,    nullptr, 0.01f, 100},
  {"vibFreq",     &P.vibFreq,     nullptr,  10,  500},
  {"vibGapFreq",  &P.vibGapFreq,  nullptr,  10,  500},
  {"vibDur",      nullptr, &P.vibDur,       5, 10000},
  {"vibMove",     &P.vibMove,     nullptr, 0.1f,    20},
  {"vibDelay",    nullptr, &P.vibDelay,     0,   300},
  {"calOX",       &P.calOX,       nullptr, -60,   60},
  {"calOY",       &P.calOY,       nullptr, -60,   60},
  {"loopPeriod",  nullptr, &P.loopPeriod,   1,    50},
  {"maxDrops",    nullptr, &P.maxFrameDrops,1,   200},
  {"hid",         nullptr, &P.hid,          0,     1},
  {"probe",       nullptr, &P.probe,        0,     1},
  {"missMin",     &P.missMin,     nullptr,  0,  2000},
  {"verbose",     nullptr, &P.verbose,      0,     1},
  {"hidLog",      nullptr, &P.hidLog,       0,     1},
  {"telHz",       nullptr, &P.telHz,        0,   100},
};
static const int N_TUNABLES = sizeof(TUNABLES)/sizeof(TUNABLES[0]);

SideState sideL = {{{0, 0.0f, -1, -1, 0.0f, false, 0.0f, 0.0f, 0.0f},
                    {0, 0.0f, -1, -1, 0.0f, false, 0.0f, 0.0f, 0.0f}}, 0.0f, 0.0f};
SideState sideR = {{{0, 0.0f, -1, -1, 0.0f, false, 0.0f, 0.0f, 0.0f},
                    {0, 0.0f, -1, -1, 0.0f, false, 0.0f, 0.0f, 0.0f}}, 0.0f, 0.0f};

int           frameDrops = 0;
unsigned long dropTotal  = 0;

int   ctKey[SENSEL_MAX_CONTACTS];    // 눌린 키 인덱스 (-1: 키캡 밖)
bool  ctGap[SENSEL_MAX_CONTACTS];    // true: 현재 등록된 키캡 두 개 사이의 틈
float ctEdge[SENSEL_MAX_CONTACTS];   // 키 중심에서의 정규화 거리 (0 중심 ~ 1 경계)
float ctDx[SENSEL_MAX_CONTACTS];     // 키 중심에서의 부호 있는 가로 오프셋(mm). +오른쪽
float ctDy[SENSEL_MAX_CONTACTS];     // 키 중심에서의 부호 있는 세로 오프셋(mm). +아래
bool  ctLeft[SENSEL_MAX_CONTACTS];   // true: 왼쪽 바 담당
int   nContacts = 0;

// ── 상시 진동 정지 감지 ──────────────────────────────────────────────────────
// 모든 접촉 ID의 기준 위치를 기억한다. 어느 하나라도 기준점에서 vibMove 이상
// 움직이면 전체 상시 진동 타이머를 다시 시작한다. 기준점은 유효 움직임이 잡힐
// 때만 갱신하므로 프레임마다 0.1mm씩 천천히 움직여도 누적되어 결국 감지된다.
#define VIB_IDLE_FADE_MS 200
int   vibAnchorId[SENSEL_MAX_CONTACTS];
float vibAnchorX[SENSEL_MAX_CONTACTS];
float vibAnchorY[SENSEL_MAX_CONTACTS];
float vibAnchorForce[SENSEL_MAX_CONTACTS];
int   vibAnchorKey[SENSEL_MAX_CONTACTS];
int   vibAnchorCount = 0;
unsigned long vibLastMotionMs = 0;
float vibContGate = 0.0f;       // 1=정상 출력, 0=정지. 끝 200ms 동안 선형 감쇠

float gDriveL = 0, gDriveR = 0;

SlotInfo infoL[SIDE_SLOTS], infoR[SIDE_SLOTS];   // 정의는 params.h

void classifyContacts() {
  nContacts = frame.n_contacts;
  if (nContacts > SENSEL_MAX_CONTACTS) nContacts = SENSEL_MAX_CONTACTS;
  for (int i = 0; i < nContacts; i++) {
    float x = frame.contacts[i].x_pos;
    ctKey[i]  = keyAt(x, frame.contacts[i].y_pos, &ctEdge[i], &ctDx[i], &ctDy[i]);
    ctGap[i]  = (ctKey[i] < 0) && activeKeyGapAt(x, frame.contacts[i].y_pos);
    ctLeft[i] = keyIsLeft(ctKey[i], x);
  }
}

// 상시 진동은 손가락이 움직이는 동안 위치 탐색 피드백을 주고, 모든 손가락이
// VIB_DUR_MS 동안 멈추면 꺼진다. 새 접촉/접촉 해제도 움직임으로 취급한다.
// 키맵에 있는 키와 그 키들 사이 틈의 접촉만 본다.
// 미매핑 키/외곽의 접촉이 다른 유효 키의 진동 타이머까지
// 다시 켜지 않게 한다.
void updateVibMotionGate() {
  int   nextId[SENSEL_MAX_CONTACTS];
  float nextX[SENSEL_MAX_CONTACTS];
  float nextY[SENSEL_MAX_CONTACTS];
  float nextForce[SENSEL_MAX_CONTACTS];
  int   nextKey[SENSEL_MAX_CONTACTS];
  int nextCount = 0;
  bool moved = false;

  for (int i = 0; i < nContacts; i++) {
    byte t = frame.contacts[i].type;
    if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
    if (ctKey[i] < 0 && !ctGap[i]) continue;

    int id = (int)frame.contacts[i].id;
    float x = frame.contacts[i].x_pos;
    float y = frame.contacts[i].y_pos;
    float force = frame.contacts[i].total_force;
    int old = -1;
    for (int j = 0; j < vibAnchorCount; j++) {
      if (vibAnchorId[j] == id) { old = j; break; }
    }

    float ax = x, ay = y, af = force;
    int akey = ctKey[i];
    if (old < 0) {
      moved = true;                         // 새 손가락
    } else {
      ax = vibAnchorX[old]; ay = vibAnchorY[old];
      af = vibAnchorForce[old]; akey = vibAnchorKey[old];

      // 압력이 달라지면 Sensel 접촉 중심도 실제 손가락 이동 없이 조금 움직인다.
      // 힘 변화가 충분히 큰 프레임에서는 그 좌표 이동을 기준점 변화로 흡수하고,
      // 활동으로 세지 않는다. 힘 자체만으로는 정지 타이머가 절대 리셋되지 않는다.
      float forceBand = 5.0f + 0.05f * fmaxf(force, af);
      bool pressureShift = fabsf(force - af) >= forceBand;
      bool crossedKey = (ctKey[i] != akey);
      // 키/틈/인접 키 판정은 경계의 좌표 노이즈만으로도 흔들릴 수 있다.
      // 진동 종류를 고르는 현재 분류만 갱신하고 정지 타이머는 리셋하지 않는다.
      // 실제 이동은 아래의 vibMove 위치 문턱으로만 판정한다.
      if (crossedKey) akey = ctKey[i];

      if (pressureShift) {
        ax = x; ay = y; af = force;          // 타이머는 건드리지 않고 좌표만 재기준
      } else if (fabsf(x - ax) >= P.vibMove ||
                 fabsf(y - ay) >= P.vibMove) {
        moved = true;
        ax = x; ay = y; af = force;          // 새 기준점에서 다시 누적
      }
    }

    if (nextCount < SENSEL_MAX_CONTACTS) {
      nextId[nextCount] = id;
      nextX[nextCount]  = ax;
      nextY[nextCount]  = ay;
      nextForce[nextCount] = af;
      nextKey[nextCount]   = akey;
      nextCount++;
    }
  }

  // 접촉이 줄어드는 것은 손가락을 뗀 것이므로 남은 손가락의 정지 타이머를
  // 다시 시작하지 않는다. 접촉이 늘어난 경우만 새로운 활동으로 본다.
  if (nextCount > vibAnchorCount) moved = true;
  vibAnchorCount = nextCount;
  for (int i = 0; i < nextCount; i++) {
    vibAnchorId[i] = nextId[i];
    vibAnchorX[i]  = nextX[i];
    vibAnchorY[i]  = nextY[i];
    vibAnchorForce[i] = nextForce[i];
    vibAnchorKey[i]   = nextKey[i];
  }

  unsigned long now = millis();
  if (moved) vibLastMotionMs = now;
  unsigned long stillMs = (unsigned long)(now - vibLastMotionMs);
  if (nextCount <= 0) {
    vibContGate = 0.0f;
  } else if (stillMs < (unsigned long)P.vibDur) {
    vibContGate = 1.0f;
  } else if (stillMs < (unsigned long)P.vibDur + VIB_IDLE_FADE_MS) {
    vibContGate = 1.0f - (float)(stillMs - (unsigned long)P.vibDur)
                             / (float)VIB_IDLE_FADE_MS;
  } else {
    vibContGate = 0.0f;
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
//
// 구간은 [데드밴드 끝점, 키캡 경계] 다 — [키 중심, 키캡 경계] 가 아니다.
// vibCenter 는 데드밴드 끝점의 세기이고, 데드밴드 안은 무음이다.
float vibAmpFor(float edge) {
  if (!useVibAxis()) return 0.0f;
  // ★ 데드밴드 안은 무음이고, vibCenter 는 '키 중심'이 아니라 '데드밴드 끝점'의
  //   세기다. edgeNorm() 이 스위트스팟을 통째로 0 으로 접어두므로, 정규화된
  //   edge 가 0 이라는 것이 곧 "데드밴드 안"이다 — 원시 거리를 따로 넘길 필요가
  //   없다. 경계 바로 바깥(edge -> 0+)에서 powf 항이 0 이 되어 정확히 vibCenter
  //   가 나오므로, 경계에서 0 -> vibCenter 로 뛰는 계단이 생긴다. 그게 의도다:
  //   스위트스팟이 "조용한 구역"이 되고 그 끝이 손끝에 또렷한 문턱으로 잡힌다.
  //
  //   deadband 가 0 이면(기본값) 이 분기는 키 중심 한 점에서만 걸려 사실상
  //   아무 일도 안 한다. 조용한 구역을 원하면 deadband 를 올려야 한다.
  //
  //   vibEdge < vibCenter 로 뒤집어 넣는 용법도 그대로 산다 — 데드밴드 안은
  //   조용하고, 경계에서 제일 세게 튀었다가 가장자리로 갈수록 잦아든다.
  if (edge <= 0.0f) return 0.0f;
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

    // ── 장전/해제 (자세한 이유는 barstate.h의 armed 주석) ──────────────────
    // 같은 이유다. 하나만 쓰면 경계에서 손끝이 떨 때 장전이 껌뻑인다.
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

// 버클링에 못 닿고 끝난 누름 하나. #probe 도 #rel 도 안 나오는 사건이라, 이 줄이
// 없으면 로그에 아무 흔적이 없다 — 손가락은 분명히 힘을 줬는데.
//   why=gap  키캡 사이 빈 띠. 임계와 무관하게 버클링이 막혀 있다(forceToZ의 onKey).
//   why=weak 키캡 위인데 그 자리 임계에 못 미쳤다. mode 2 에서 엣지 임계가
//            올라간 만큼이 여기로 떨어진다.
//   key    기준 키. gap 이면 nearestKey() 가 고른 제일 가까운 키라 "어느 틈"인지가 읽힌다.
//   edge   그 기준 키 중심으로부터의 정규화 거리. gap 은 1.0 을 넘는다(키캡 밖).
//   f      이 에피소드의 최대 원시 힘. af는 그 프레임에 버클링 곡선이 실제로 본 힘.
//   th     그 자리에서 넘어야 했던 임계.
//   x      af/th. 1 에 가까울수록 아깝게 놓친 타건이다. gap 줄에서는 뜻이 없다 —
//          거기는 임계를 아무리 낮춰도 안 눌리므로 th 는 "그 자리의 뻣뻣함"일 뿐이다.
//   px/py  센서 원좌표(mm). dx/dy 는 기준 키에 묶여 있으므로, 키맵과 무관하게
//          틈의 분포를 다시 그려보려면 이쪽이 필요하다.
void probeMiss(ContactState* c) {
  if (!P.probe || P.missMin <= 0.0f || c->missMax < P.missMin) return;
  float th = P.fPeak * forceScale(edgeNorm(c->missEdge));
  Serial.print("#miss key=");
  Serial.print(c->missKey >= 0 ? KEYS[c->missKey].label : "-");
  Serial.print(" why=");  Serial.print(c->missOn ? "weak" : "gap");
  Serial.print(" edge="); Serial.print(c->missEdge, 2);
  Serial.print(" mm=");   Serial.print(c->missEdge * 8.5f, 1);
  Serial.print(" dx=");   Serial.print(c->missDx, 1);
  Serial.print(" dy=");   Serial.print(c->missDy, 1);
  Serial.print(" px=");   Serial.print(c->missPx, 1);
  Serial.print(" py=");   Serial.print(c->missPy, 1);
  Serial.print(" th=");   Serial.print(th, 0);
  Serial.print(" f=");    Serial.print(c->missMax, 0);
  Serial.print(" af=");   Serial.print(c->missAct, 0);
  Serial.print(" x=");    Serial.println(th > 0.0f ? c->missAct / th : 0.0f, 2);
}

// 에피소드를 접고 비운다. 다음 누름을 받을 준비까지 여기서 끝낸다.
void missClear(ContactState* c) {
  c->missMax = 0.0f; c->missAct = 0.0f;
  c->missEdge = 0.0f; c->missDx = 0.0f; c->missDy = 0.0f;
  c->missPx = 0.0f;  c->missPy = 0.0f;   c->missKey = -1;  c->missOn = false;
}

void assignSlots(SideState* s, bool leftSide, int outIdx[SIDE_SLOTS]) {
  for (int k = 0; k < SIDE_SLOTS; k++) outIdx[k] = -1;

  // 버클링 중인 접촉만 ID를 고정한다. 버클링 전의 레스팅 접촉까지
  // 손가락을 떼 때까지 고정하면 좌/우 2개 슬롯을 먼저 올려놓은 손가락이
  // 영원히 차지해, 나머지 손가락을 세게 눌러도 보지 못한다.
  for (int k = 0; k < SIDE_SLOTS; k++) {
    if (s->c[k].activeId < 0) continue;
    int found = -1;
    for (int i = 0; i < nContacts; i++) {
      byte t = frame.contacts[i].type;
      if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
      if (ctLeft[i] != leftSide) continue;
      if ((int)frame.contacts[i].id == s->c[k].activeId) { found = i; break; }
    }
    if (found >= 0 && s->c[k].buttonState) outIdx[k] = found;
    if (found >= 0) continue;   // 버클링 전이면 아래 힘 순위에서 다시 경쟁
    // 담당하던 접촉이 사라짐. buttonState를 여기서 0으로 되돌리므로 릴리즈 "엣지"는
    // 사라진다. 상태 재구성 방식이라 상관없다 — heldKey만 비워두면 다음 리포트에서
    // 그 키가 빠진다.
    if (outIdx[k] < 0) {
      probeRelease(&s->c[k]);            // 손가락을 통째로 뗀 경우가 여기다
      probeMiss(&s->c[k]);               // 버클링 못 한 채 손을 뗀 경우가 여기다.
                                         // 힘이 문턱 위에 머무른 채 접촉이 사라지면
                                         // 아래 sideDrive() 의 "힘이 빠졌다" 경로가
                                         // 영영 안 걸리므로 여기서도 닫아야 한다.
      s->c[k].activeId = -1; s->c[k].buttonState = 0; s->c[k].prevForce = 0.0f;
      s->c[k].heldKey = -1;  s->c[k].heldEdge = 0.0f;  s->c[k].maxForce = 0.0f;
      missClear(&s->c[k]);   s->c[k].missArmed = true;
      s->c[k].wasOnKey = false; s->c[k].entryForce = 0.0f;
    }
  }

  // 버클링되지 않은 빈 슬롯은 매 프레임 힘이 큰 접촉에게 준다.
  // 따라서 10손가락을 레스팅해도 실제로 누르기 시작한 손가락이
  // 기존 레스팅 손가락을 밀어내고 슬롯을 받을 수 있다.
  int candidates[SIDE_SLOTS];
  bool candidateUsed[SIDE_SLOTS];
  int freeSlots = 0;
  for (int c = 0; c < SIDE_SLOTS; c++) { candidates[c] = -1; candidateUsed[c] = false; }
  for (int k = 0; k < SIDE_SLOTS; k++) if (outIdx[k] < 0) freeSlots++;
  for (int c = 0; c < freeSlots; c++) {
    int best = -1; float bestF = 0.0f;
    for (int i = 0; i < nContacts; i++) {
      byte t = frame.contacts[i].type;
      if (t == SENSEL_CONTACT_TYPE_INVALID || t == SENSEL_CONTACT_TYPE_END) continue;
      if (ctLeft[i] != leftSide) continue;
      bool taken = false;
      for (int j = 0; j < SIDE_SLOTS; j++) {
        if (outIdx[j] == i || candidates[j] == i) { taken = true; break; }
      }
      if (taken) continue;
      float f = frame.contacts[i].total_force;
      if (best < 0 || f > bestF) { best = i; bestF = f; }
    }
    if (best < 0) break;
    candidates[c] = best;
  }

  // 상위 후보 안에 기존 ID가 아직 있으면 같은 슬롯에 두어 힘 순위가
  // 조금 바뀔 때마다 두 슬롯이 서로 바뀌는 것을 막는다.
  for (int k = 0; k < SIDE_SLOTS; k++) {
    if (outIdx[k] >= 0 || s->c[k].activeId < 0) continue;
    for (int c = 0; c < SIDE_SLOTS; c++) {
      int i = candidates[c];
      if (candidateUsed[c] || i < 0) continue;
      if ((int)frame.contacts[i].id == s->c[k].activeId) {
        outIdx[k] = i; candidateUsed[c] = true; break;
      }
    }
  }

  for (int k = 0; k < SIDE_SLOTS; k++) {
    if (outIdx[k] >= 0) continue;
    int best = -1;
    for (int c = 0; c < SIDE_SLOTS; c++) {
      if (!candidateUsed[c] && candidates[c] >= 0) {
        best = candidates[c]; candidateUsed[c] = true; break;
      }
    }
    if (best >= 0) {
      int newId = (int)frame.contacts[best].id;
      outIdx[k] = best;
      if (s->c[k].activeId != newId) {
        s->c[k].activeId = newId;
        s->c[k].buttonState = 0; s->c[k].prevForce = 0.0f;
        s->c[k].heldKey = -1; s->c[k].heldEdge = 0.0f; s->c[k].maxForce = 0.0f;
        missClear(&s->c[k]); s->c[k].missArmed = true;   // 새 손가락 = 새 에피소드
        // 레스팅 손가락을 밀어내고 뒤늦게 선택된 타건은 현재 힘을
        // 그대로 보아야 한다. 여기서 entryForce로 빼면 슬롯을 받은 시점의
        // 누름이 전부 사라져 또 더 눌러야만 버클링한다.
        s->c[k].wasOnKey = (ctKey[best] >= 0);
        s->c[k].entryForce = 0.0f;
      }
    }
  }
}

// 한쪽 손의 슬롯 2개로 바 구동값을 계산한다. out[]에 이번 프레임 상태를 남긴다.
float sideDrive(SideState* s, const int idx[SIDE_SLOTS], SlotInfo out[SIDE_SLOTS]) {
  float z = 0.0f;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    float force = 0.0f, edgeRaw = 0.0f;
    float dxRaw = 0.0f, dyRaw = 0.0f;   // #probe 에만 실린다. 햅틱은 안 쓴다
    float pxRaw = 0.0f, pyRaw = 0.0f;   // 센서 원좌표(mm). #miss 에만 실린다
    int   key   = -1;
    bool  inGap = false;
    if (idx[k] >= 0) {
      force   = frame.contacts[idx[k]].total_force;
      key     = ctKey[idx[k]];
      edgeRaw = ctEdge[idx[k]];
      dxRaw   = ctDx[idx[k]];
      dyRaw   = ctDy[idx[k]];
      pxRaw   = frame.contacts[idx[k]].x_pos;
      pyRaw   = frame.contacts[idx[k]].y_pos;
      inGap   = ctGap[idx[k]];
    }

    // 데드밴드를 뺀 값이 아래 전부(임계 배수, 진폭, 프리로드, 래치)의 기준이 된다.
    // 갭이면 keyAt()이 edgeRaw=1을 주므로 자동으로 제일 뻣뻣하고 제일 약한 쪽이 된다.
    float edge  = edgeNorm(edgeRaw);
    bool  onKey = (key >= 0);

    // 갭 -> 키 진입 첫 프레임의 total_force는 갭/하우징에 이미 실려
    // 있던 힘으로 본다. 키 곡선은 그 뒤 추가로 증가한 힘만 받는다.
    // 진입 후 버클링 전에 힘을 덜었다면 그 최저값을 새 기준으로
    // 삼아, 다시 누른 증가량을 모두 인정한다.
    if (!s->c[k].buttonState) {
      if (onKey && !s->c[k].wasOnKey) {
        s->c[k].entryForce = force;
      } else if (onKey && s->c[k].entryForce > 0.0f && force < s->c[k].entryForce) {
        s->c[k].entryForce = force;
      }
    }
    float actForce = force;
    // 버클링 후 갭으로 미끄러져도 릴리즈까지 같은 기준점을 유지한다.
    if (s->c[k].entryForce > 0.0f) {
      actForce -= s->c[k].entryForce;
      if (actForce < 0.0f) actForce = 0.0f;
    }
    s->c[k].wasOnKey = onKey;

    int prevBS = s->c[k].buttonState;
    // 누르고 있는 동안 최대힘을 쌓는다. forceToZ()가 상태를 바꾸기 전에 봐야
    // 릴리즈 직전 프레임의 힘까지 들어간다.
    if (prevBS && force > s->c[k].maxForce) s->c[k].maxForce = force;
    // 접촉이 없어도(force=0) 호출해서 버클링 상태를 풀어줘야 한다.
    float unit = forceToZ(&s->c[k], actForce, edge, onKey);
    if (prevBS && !s->c[k].buttonState) {   // 힘이 빠져 풀린 경우
      probeRelease(&s->c[k]);               // heldKey는 아래에서 비워지므로 지금 찍는다
      s->c[k].maxForce = 0.0f;
    }

    // ── 미인식 타건 에피소드 ────────────────────────────────────────────────
    // 버클링을 못 한 채 힘만 오르내린 구간 하나를 모았다가, 힘이 빠지면 #miss 로
    // 뱉는다. 여닫는 문턱을 둘로 벌린 건 클릭 곡선의 fPeak/fValley 와 같은 이유다 —
    // 문턱 하나만 쓰면 손끝이 그 근처에서 떨 때 누름 한 번이 여러 줄로 쪼개진다.
    //
    // 위치는 "최대 힘이 나온 프레임"의 것을 쓴다. 접촉은 누르는 내내 미끄러지므로
    // 시작·끝 좌표보다 이쪽이 사람이 겨냥한 자리에 가깝다 (#probe 가 버클링 순간의
    // 좌표를 쓰는 것과 같은 취지다).
    const float missClose = P.missMin * 0.5f;
    if (s->c[k].buttonState) {
      // 결국 버클링했다. 이 누름은 #probe/#rel 로 나가므로 미인식이 아니다.
      missClear(&s->c[k]);
      s->c[k].missArmed = false;
    } else if (P.missMin > 0.0f) {
      if (!s->c[k].missArmed) {
        // 방금 버클링이 풀린 참이다. 힘이 충분히 빠질 때까지는 새 에피소드를
        // 안 연다 — 안 그러면 정상 타건을 뗄 때마다 #miss 가 하나씩 따라붙는다.
        if (force < missClose) s->c[k].missArmed = true;
      } else if (force > s->c[k].missMax) {
        s->c[k].missMax  = force;
        s->c[k].missAct  = actForce;
        s->c[k].missOn   = onKey;
        s->c[k].missKey  = key;
        s->c[k].missEdge = edgeRaw;
        s->c[k].missDx   = dxRaw;   s->c[k].missDy = dyRaw;
        s->c[k].missPx   = pxRaw;   s->c[k].missPy = pyRaw;
        if (!onKey && idx[k] >= 0) {
          // 갭이라 keyAt() 이 준 건 edge=1, dx=dy=0 짜리 자리표시자뿐이다.
          // 제일 가까운 키를 기준으로 삼아야 "어느 틈을 눌렀나"가 남는다.
          float ndx, ndy;
          int   nk = nearestKey(pxRaw, pyRaw, &ndx, &ndy);
          if (nk >= 0) {
            s->c[k].missKey  = nk;
            s->c[k].missDx   = ndx;  s->c[k].missDy = ndy;
            s->c[k].missEdge = keyEdgeOf(nk, ndx, ndy);
          }
        }
      } else if (s->c[k].missMax > 0.0f && force < missClose) {
        probeMiss(&s->c[k]);        // 힘이 빠졌다 = 이 누름은 끝났다
        missClear(&s->c[k]);
      }
    }

    // 버클링 후에는 래치된 위치가 진짜 임계와 세기를 정한다.
    float eEff = s->c[k].buttonState ? s->c[k].heldEdge : edge;

    // 버클링 전이에서 키를 래치/해제한다. 이 순간이 곧 손가락이 클릭을 느끼는 순간이고,
    // fPeak/fValley 히스테리시스가 그대로 디바운스 역할을 한다.
    bool down = (!prevBS &&  s->c[k].buttonState);   // 키다운
    bool up   = ( prevBS && !s->c[k].buttonState);   // 키업
    // forceToZ()는 유효 힘을 받으므로 버클링 첫 프레임의 maxForce는
    // 여기서 센서 원시 힘으로 다시 잡는다. #rel fmax의 기존 의미를 유지한다.
    if (down) s->c[k].maxForce = force;
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
    // vibWhen 3(상시)은 버스트를 아예 안 쓴다 — 아래 vibCont 쪽으로 나간다.
    bool burstMode = (P.vibWhen <= 2);
    if (useVibAxis() && burstMode && ((P.vibWhen != 1 && down) || (P.vibWhen != 0 && up))) {
      unsigned long nowUs = micros();
      float amp = vibAmpFor(s->c[k].heldEdge);       // powf는 임계구역 밖에서
      // vibISR()이 이 두 필드를 읽고 버스트가 끝나면 vibT0을 0으로 지운다.
      // 따로 쓰면 그 사이에 ISR이 끼어들어 새 vibT0에 이전 vibAmp가 붙는다.
      noInterrupts();
      s->c[k].vibT0  = nowUs ? nowUs : 1;            // 0은 "비활성" 표시라 피한다
      s->c[k].vibAmp = amp;
      interrupts();
    }

    if (down)     s->c[k].heldKey = key;
    else if (up)  s->c[k].heldKey = -1;

    // 버클링 후에는 래치된 키를 따라간다 — 접촉 중심이 키 경계를 들락거려도
    // (키 사이 2.9mm 빈 띠) 바가 껐다 켜졌다 하지 않는다.
    bool  live = s->c[k].buttonState ? (s->c[k].heldKey >= 0) : onKey;
    float gain = ampGain(eEff);   // mode 0에서는 위치와 무관하게 1.0
    float amp  = live ? gain : 0.0f;

    // ── 진동 세기가 보는 위치 ─────────────────────────────────────────────
    // ★ 상시(vibWhen 3)만 eEff 가 아니라 실시간 edge 를 쓴다.
    //
    //   eEff 는 버클링하는 순간 heldEdge 로 래치된다. 임계힘과 클릭 세기는
    //   그래야 맞다 — 누른 채 엣지로 미끄러졌다고 임계가 올라가 키가 저절로
    //   풀리면 안 되니까 (heldEdge 주석 참고).
    //
    //   그런데 상시 모드에서는 그 래치가 정확히 기능을 죽인다. 누른 채로
    //   손가락을 중심에 다시 맞춰도 진동이 그대로 유지돼서, "지금 내가 얼마나
    //   빗나가 있나"를 실시간으로 못 읽는다. 이 모드의 존재 이유가 위치를
    //   손끝으로 계속 추적하는 것이므로 여기서는 살아있는 edge 를 써야 한다.
    //
    //   클릭 쪽(forceScale/ampGain)은 그대로 eEff 다. 진동만 실시간이 되고
    //   버클링 히스테리시스는 손대지 않으므로, 키가 풀리거나 임계가 흔들리는
    //   부작용은 없다.
    float vibEdgeNow = (P.vibWhen == 3) ? edge : eEff;

    // ── vibWhen 3 (상시) ──────────────────────────────────────────────────
    // 버클링을 아예 안 본다. 접촉이 움직이는 동안 현재 편심을 계속 실어 보낸다.
    // 모든 접촉이 vibDur 동안 멈추면 vibContGate가 200ms 동안 1 -> 0으로 내려가고,
    // 기존 5ms 스무딩까지 거쳐 부드럽게 꺼진다. 다시 움직이면 즉시 켜진다.
    //
    // 현재 키맵의 키캡 위에서는 편심 세기/주파수를, 그 키캡들
    // 사이 틈에서는 vibGap/vibGapFreq를 쓴다. 주석 처리된 미사용
    // 키 자리와 키보드 외곽은 둘 다 아니므로 조용하다.
    float vibContNow = 0.0f;
    bool  vibContIsGap = false;
    if (useVibAxis() && P.vibWhen == 3 && vibContGate > 0.0f && idx[k] >= 0) {
      if (onKey) {
        vibContNow = vibAmpFor(vibEdgeNow) * vibContGate;
      } else if (inGap) {
        vibContNow = P.vibGap * 0.001f * vibContGate;
        vibContIsGap = true;
      }
    }
    s->c[k].vibContGap = vibContIsGap;
    s->c[k].vibCont = vibContNow;

    out[k].key     = key;
    out[k].edgeRaw = edgeRaw;
    out[k].force   = force;
    out[k].peak    = P.fPeak * forceScale(eEff);
    out[k].gain    = gain;
    // 텔레메트리도 같은 값을 봐야 한다. 상시에서 eEff 를 그대로 쓰면 눌린 동안
    // #tel 의 vib= 만 옛날 값에 얼어붙어서 실제 출력과 어긋난다.
    float vibNow = 0.0f;
    if (useVibAxis() && idx[k] >= 0) {
      if (onKey) vibNow = vibAmpFor(vibEdgeNow);
      else if (inGap) vibNow = P.vibGap * 0.001f;
    }
    out[k].vib = (P.vibWhen == 3) ? vibNow * vibContGate : vibNow;

    if (P.probe && !prevBS && s->c[k].buttonState) {
      Serial.print("#probe key=");
      Serial.print(key >= 0 ? KEYS[key].label : "-");
      Serial.print(" edge="); Serial.print(edgeRaw, 2);
      Serial.print(" mm=");   Serial.print(edgeRaw * 8.5f, 1);
      // dx/dy = 부호 있는 오프셋(mm). mm 은 이 둘을 max 로 뭉갠 크기라 방향이
      // 없다. 키별 편향을 보려면(그래서 키 중심을 옮기려면) 부호가 필요하다.
      Serial.print(" dx=");   Serial.print(dxRaw, 1);
      Serial.print(" dy=");   Serial.print(dyRaw, 1);
      Serial.print(" eff=");  Serial.print(edge, 2);
      Serial.print(" th=");   Serial.print(out[k].peak, 0);
      Serial.print(" gain="); Serial.print(gain, 2);
      Serial.print(" vib=");  Serial.print(out[k].vib, 3);
      // f/x가 버클링 곡선이 실제로 본 힘을 나타내야 한다. 갭에서
      // 진입한 누름은 원시 힘에서 entryForce를 뺀 actForce가 그 값이다.
      Serial.print(" f=");    Serial.print(actForce, 0);
      Serial.print(" x=");    Serial.println(actForce / out[k].peak, 1);
    }

    s->c[k].prevForce = force;
    // up이 되면 buttonState가 0이므로 다음 프레임부터 슬롯은 즉시 다시
    // 경쟁 가능하다. activeId는 같은 손가락의 릴리즈 꼬리를 #miss로
    // 오인하지 않게 식별자로만 남겨두며, 다른 손가락이 더 세면 바로 교체된다.
    z += P.clickScale * unit * amp;
  }
  return z;
}

// ── mode 3 진동 합성 ──────────────────────────────────────────────────────────
// ★ 이 아래 셋은 loop()가 아니라 vibISR() (하드웨어 타이머) 에서만 불린다.
//   loop()에 두면 frameStep()의 블로킹 때문에 200Hz 샘플-홀드가 된다.
//   이유는 vibISR() 위 주석에 자세히 적어뒀다.

// 슬롯 하나가 지금 내야 할 진동 "진폭"(부호 없는 봉투). 사인은 여기서 곱하지
// 않는다 — 같은 바에 물린 두 슬롯이 각자 위상을 갖고 더해지면 서로 간섭해서
// 세기가 들쭉날쭉해지므로, 진폭만 더한 뒤 바깥에서 사인 하나를 곱한다.
float vibSlotAmp(ContactState* c, unsigned long nowUs) {
  if (!useVibAxis()) { c->vibT0 = 0; return 0.0f; }

  // 여기는 버스트(vibWhen 0/1/2) 전용이다. 상시(3)는 발사 자체를 안 하므로
  // vibT0이 0으로 남아 그대로 빠져나간다 — 그쪽은 vibContAmps()가 맡는다.
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

// vibWhen 3(상시)의 진폭. 한쪽 바에 잡힌 접촉 중 진폭이 가장 큰 하나만
// 선택한다. 손가락 수에 따라 진폭이 더해져 커지지 않게 하기 위함이다.
// 선택된 접촉이 키캡 위인지 틈인지에 따라 해당 목표치만 켜고, 두 성분은
// 각각 한 극점(one-pole)으로 따라가므로 승자가 바뀔 때도 부드럽게 교차한다.
//
// 스무딩이 필요한 이유: vibCont는 sideDrive()가 200Hz 프레임마다 계단으로 쓴다.
// 그걸 그대로 사인에 곱하면 진폭이 200Hz로 계단을 밟고, 그 계단이 다시 200Hz
// 성분이 되어 실려 나온다 — ISR로 옮겨서 없앤 그 artifact가 진폭 축으로 되돌아
// 오는 셈이다. 버스트(vibWhen 0/1/2)는 이미 sin 봉투라 매끄러우므로 여기를
// 안 거친다. 상시 성분만 통과시킨다.
//
// 시정수는 프레임 간격과 같은 5ms로 잡았다. 이보다 길면 손가락을 따라오는 게
// 눈에 띄게 늦고(어루만지는 게 목적이라 치명적), 짧으면 계단이 그대로 남는다.
#define VIB_CONT_TAU_S 0.005f

void vibContAmps(SideState* s, float dt, float* keyAmp, float* gapAmp) {
  float maxTarget = 0.0f;
  bool  maxIsGap = false;
  for (int k = 0; k < SIDE_SLOTS; k++) {
    if (s->c[k].vibCont > maxTarget) {
      maxTarget = s->c[k].vibCont;
      maxIsGap  = s->c[k].vibContGap;
    }
  }
  float keyTarget = maxIsGap ? 0.0f : maxTarget;
  float gapTarget = maxIsGap ? maxTarget : 0.0f;
  float a = dt / VIB_CONT_TAU_S;
  s->vibContS    += (keyTarget - s->vibContS)    * a;
  s->vibGapContS += (gapTarget - s->vibGapContS) * a;
  // 지수 꼬리를 끊는다. 안 그러면 손을 뗀 뒤에도 아주 작은 값이 남아 vibSide()의
  // "amp <= 0" 분기에 영영 안 걸리고 위상이 계속 돈다.
  if (s->vibContS < 1.0e-4f) s->vibContS = 0.0f;
  if (s->vibGapContS < 1.0e-4f) s->vibGapContS = 0.0f;
  *keyAmp = s->vibContS;
  *gapAmp = s->vibGapContS;
}

// 키캡 위와 틈 주파수의 위상을 각각 누적시킨다.
//   ★ 위상을 micros()에서 매번 다시 만들지 않는 이유: float 가수가 24비트라
//     micros()가 커지면 t*1e-6 의 분해능이 주기보다 굵어져 주파수가 흔들린다.
//     dt를 적분하면 그 문제가 없다.
float vibWave(float amp, float freq, float* phase, float dt) {
  if (amp <= 0.0f) { *phase = 0.0f; return 0.0f; }   // 조용할 땐 위상도 리셋
  *phase += 2.0f * PI * freq * dt;
  if (*phase > 2.0f * PI) *phase -= 2.0f * PI;
  return amp * sinf(*phase);
}

float vibSide(SideState* s, float* keyPhase, float* gapPhase,
              unsigned long nowUs, float dt, float testAmp) {
  float keyAmp = testAmp;
  for (int k = 0; k < SIDE_SLOTS; k++) keyAmp += vibSlotAmp(&s->c[k], nowUs);
  float keyCont, gapAmp;
  vibContAmps(s, dt, &keyCont, &gapAmp);
  keyAmp += keyCont;
  return vibWave(keyAmp, P.vibFreq, keyPhase, dt)
       + vibWave(gapAmp, P.vibGapFreq, gapPhase, dt);
}

// ── 진동 테스트 ('vib' 명령) ─────────────────────────────────────────────────
// 접촉도 버클링도 mode도 보지 않고 양쪽 바를 그냥 흔든다. "진동이 안 느껴진다"가
// 액추에이터/주파수 문제인지 트리거가 안 걸리는 문제인지를 가르는 게 목적이다.
// 이게 느껴지는데 타건에서 안 느껴지면 트리거·세기·마스킹 쪽이고,
// 이것조차 안 느껴지면 vibFreq가 이 바의 기계적 대역 밖이다.
unsigned long vibTestT0  = 0;      // 0 = 꺼짐
unsigned long vibTestDur = 0;      // us. 0 = 끌 때까지 계속
float         vibTestAmp = 0.0f;

// ★ ISR 한 번에 한 번만 불러야 한다 (좌/우에서 각각 부르면 첫 호출이 상태를 끄고
//   두 번째는 0을 받는다). vibISR()에서 한 번 계산해 양쪽에 나눠준다.
float vibTestNow(unsigned long nowUs) {
  if (!vibTestT0) return 0.0f;
  long t = (long)(nowUs - vibTestT0);
  if (vibTestDur == 0) return vibTestAmp;                 // 지속 — 봉투 없음
  if (t >= (long)vibTestDur) { vibTestT0 = 0; return 0.0f; }
  return vibTestAmp * sinf(PI * (float)t / (float)vibTestDur);
}

float vibPhaseL = 0.0f, vibPhaseR = 0.0f;
float vibGapPhaseL = 0.0f, vibGapPhaseR = 0.0f;
float gVibL = 0.0f, gVibR = 0.0f;   // 텔레메트리용 마지막 값

// 좌우 액추에이터의 기계적 편차 보정. 버클링과 진동을 합친 뒤 PWM 변환 전에 적용한다.
const float LEFT_GAIN = 0.90f;

float clampDrive(float x){ if (x > 1.0f) return 1.0f; if (x < -1.0f) return -1.0f; return x; }

// ── 진동 합성 ISR ─────────────────────────────────────────────────────────────
// ★ 진동은 loop()가 아니라 하드웨어 타이머에서 만든다. loop()에 두면 안 된다:
//
//   frameStep() 안의 senselGetFrame()이 블로킹이다. READ_FRAME을 보내고 헤더
//   5바이트를 busy-wait 하는데, USB full-speed 왕복과 Morph의 200Hz 스캔 경계가
//   겹쳐 매 프레임 수 ms가 그냥 날아간다. 그 동안 moveBar()가 안 불리므로 PWM은
//   직전 듀티에 고정된다 — 진동이 아니라 DC 전류가 코일에 흐른다.
//
//   게다가 loopTimer 리셋이 frameStep() '앞'이라 요청이 정확히 5ms 간격으로
//   나가고, Morph 스캔도 5ms 간격이다. 두 클럭이 위상 고정돼서 이 공백 길이가
//   부팅 때 걸린 위상대로 굳는다 — 0.2ms일 수도 4.5ms일 수도 있고, 크리스털
//   드리프트로 수십 초에 걸쳐 훑는다. "어떤 날은 느껴지고 어떤 날은 전혀 안
//   느껴지는" 증상이 여기서 나온다.
//
//   결과적으로 출력이 200Hz 샘플-홀드가 되어 220Hz 사인이 |220-200| = 20Hz로
//   접힌다. 층을 나누는 것만으로는 못 고친다 — 같은 스레드면 소용이 없다.
//
//   dt를 micros() 차이가 아니라 상수로 두는 이유: 타이머는 평균적으로 정확히
//   VIB_ISR_HZ로 뛰므로, ISR 지터를 dt에 반영하면 오히려 그게 주파수 지터가 된다.
//   상수 dt 쪽이 주파수가 더 정확하다. 증분이 2π를 넘지 않는 것도 보장된다
//   (vibFreq 상한 500Hz에서 2π*500/2000 = π rad < 2π) — vibSide()의 위상 랩이
//   한 번 빼기로 끝나는 근거다.
#define VIB_ISR_HZ 2000               // 220Hz 한 주기에 약 9샘플. CPU 약 0.4%.
IntervalTimer vibTimer;

void vibISR() {
  unsigned long nowUs = micros();     // 봉투 타이밍은 절대시간이라 그대로 쓴다
  const float dt = 1.0f / (float)VIB_ISR_HZ;

  float testAmp = vibTestNow(nowUs);  // ISR 한 번에 한 번만 (좌/우가 나눠 쓴다)
  gVibL = vibSide(&sideL, &vibPhaseL, &vibGapPhaseL, nowUs, dt, testAmp);
  gVibR = vibSide(&sideR, &vibPhaseR, &vibGapPhaseR, nowUs, dt, testAmp);

  // 두 바를 각각 갱신 -> 양손 동시 입력이 서로를 막지 않는다.
  moveBar1(clampDrive((gDriveL + gVibL) * LEFT_GAIN));
  moveBar2(clampDrive(gDriveR + gVibR));
}

// mode나 곡선을 바꾸면 물려 있던 버클링 상태가 새 파라미터와 안 맞는다.
// (예: 임계를 올린 순간 이미 버클링한 접촉이 릴리즈 임계 위에 떠 버린다)
// 전부 풀고 다시 쌓게 한다. 손가락이 올라가 있어도 다음 프레임에 복구된다.
void resetBuckling() {
  // vibISR()이 같은 vibT0/vibPhase/vibTest*를 읽고 PWM을 쓴다. 통째로 막는다.
  noInterrupts();
  SideState* sides[2] = { &sideL, &sideR };
  for (int s = 0; s < 2; s++)
    for (int k = 0; k < SIDE_SLOTS; k++) {
      sides[s]->c[k].buttonState = 0;
      sides[s]->c[k].heldKey     = -1;
      sides[s]->c[k].heldEdge    = 0.0f;
      sides[s]->c[k].maxForce    = 0.0f;
      sides[s]->c[k].wasOnKey    = false;
      sides[s]->c[k].entryForce  = 0.0f;
      missClear(&sides[s]->c[k]);              // 임계가 바뀌면 모아둔 에피소드도 못 믿는다
      sides[s]->c[k].missArmed   = true;
      sides[s]->c[k].vibT0       = 0;
      sides[s]->c[k].vibAmp      = 0.0f;
      sides[s]->c[k].vibCont     = 0.0f;
      sides[s]->c[k].vibContGap  = false;
    }
  sideL.vibContS = sideR.vibContS = 0.0f;
  sideL.vibGapContS = sideR.vibGapContS = 0.0f;
  vibAnchorCount = 0;
  vibLastMotionMs = millis();
  vibContGate = 0.0f;
  vibPhaseL = vibPhaseR = 0.0f;
  vibGapPhaseL = vibGapPhaseR = 0.0f;
  gVibL = gVibR = 0.0f;
  vibTestT0 = 0; vibTestAmp = 0.0f;
  // ★ 클릭 값도 같이 지워야 한다. 안 그러면 아래 moveBar(0)을 vibISR이 다음
  //   틱(100us)에 clampDrive(gDrive + 0)으로 곧바로 덮어써서 바가 안 풀린다.
  //   loop()에서 돌던 시절에도 같은 구조였지만, 그땐 frameStep()이 곧 gDrive를
  //   다시 계산해줘서 드러나지 않았다.
  gDriveL = gDriveR = 0.0f;
  moveBar1(0); moveBar2(0);
  interrupts();
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

  // 곡선의 의미 자체가 바뀌는 것. vibWhen도 포함이다 — 버스트와 상시는 상태
  // 변수가 서로 달라서, 갈아탈 때 반대쪽 잔여물(진행 중인 버스트 / 남은 vibCont)을
  // 지워주지 않으면 바뀐 직후 한동안 두 모드가 섞여 나간다.
  bool structural = (t->i == &P.mode) || (t->i == &P.vibWhen);
  int oldVibWhen = P.vibWhen;
  if (t->f) *t->f = v; else *t->i = (int)lroundf(v);

  // 한 파라미터를 두 용도로 쓰되, 모드를 바꿀 때 흔한 기본값은 자동으로 맞춘다.
  // 사용자가 50/2000 이외의 값을 직접 정했다면 의도를 보존해 건드리지 않는다.
  if (t->i == &P.vibWhen) {
    if (oldVibWhen != 3 && P.vibWhen == 3 && P.vibDur == 50) P.vibDur = 2000;
    if (oldVibWhen == 3 && P.vibWhen != 3 && P.vibDur == 2000) P.vibDur = 50;
  }

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
  const uint32_t P_MAGIC_V11 = 0x56434D3BUL;
  const uint32_t P_MAGIC_V10 = 0x56434D3AUL;
  bool migrateV11 = (tmp.magic == P_MAGIC_V11);
  bool migrateV10 = (tmp.magic == P_MAGIC_V10);
  if (tmp.magic != P_MAGIC && !migrateV11 && !migrateV10) {
    if (!quiet) Serial.println("#err eeprom empty");
    return;
  }
  // VCM10의 vibDur는 상시 모드에서 무시되던 값이라 2초로 옮긴다.
  // VCM10/11은 구조 끝에 vibMove가 없었으므로 기존 고정값 5mm를 채운다.
  if (migrateV11 || migrateV10) {
    if (migrateV10 && tmp.mode == 3 && tmp.vibWhen == 3) tmp.vibDur = 2000;
    tmp.vibMove = 5.0f;
    tmp.magic = P_MAGIC;
    EEPROM.put(0, tmp);
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
      noInterrupts();
      vibTestT0 = 0; vibTestAmp = 0.0f;
      interrupts();
      Serial.println("#ok vibtest off");
    } else {
      unsigned long now = micros();
      unsigned long dur = (ms > 0) ? (unsigned long)ms * 1000UL : 0UL;
      float amp = a * 0.001f;
      noInterrupts();                 // 세 값이 vibISR에 한 벌로 보여야 한다
      vibTestT0  = now ? now : 1;
      vibTestDur = dur;
      vibTestAmp = amp;
      interrupts();
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

  applyDefaults(2);            // EEPROM이 없을 때는 test.html의 force-8 튜닝값
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
    missClear(&sideL.c[k]);     missClear(&sideR.c[k]);
    sideL.c[k].missArmed = true; sideR.c[k].missArmed = true;
    sideL.c[k].heldEdge = 0.0f; sideR.c[k].heldEdge = 0.0f;
    sideL.c[k].maxForce = 0.0f; sideR.c[k].maxForce = 0.0f;
    sideL.c[k].wasOnKey = false; sideR.c[k].wasOnKey = false;
    sideL.c[k].entryForce = 0.0f; sideR.c[k].entryForce = 0.0f;
  }
  frame.n_contacts = 0; nContacts = 0; frameDrops = 0; dropTotal = 0;

  // ★ 진동 타이머는 반드시 여기서 켠다 — initializeBars()로 핀·PWM이 잡히고
  //   loadFromEeprom() 안의 resetBuckling()이 끝난 뒤여야 한다. 그 전에 켜면
  //   ISR이 설정 안 된 핀에 analogWrite를 한다.
  //   priority(64): 기본 128은 USBHost ISR과 같은 등급이라 긴 USB 처리에 밀려
  //   지터가 생긴다. vibISR은 ~2us로 짧아서 앞질러도 USB를 방해하지 않는다.
  vibTimer.priority(64);
  vibTimer.begin(vibISR, 1000000 / VIB_ISR_HZ);

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

// loop()는 센서 층만 돈다 — 센서 읽기 · 클릭 계산 · HID · 로그.
// 진동 합성과 바 출력은 vibISR() (VIB_ISR_HZ 하드웨어 타이머) 이 맡는다.
// frameStep() 안의 senselGetFrame()이 블로킹이라 같은 스레드에 두면 매 프레임
// 수 ms 동안 바 출력이 멈추고, 그 결과 220Hz가 20Hz로 접힌다 — vibISR() 주석 참고.
//
// 클릭 값(gDriveL/R)은 여전히 loopPeriod 주기로만 갱신된다. 그건 원래 그랬고
// (센서가 200Hz다), ISR은 그 위에 진동만 얹어서 내보낸다.
void loop() {
  if (loopTimer >= (unsigned long)P.loopPeriod) {
    loopTimer = 0;
    frameStep();
  }
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
  updateVibMotionGate();

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
