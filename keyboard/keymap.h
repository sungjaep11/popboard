#ifndef KEYMAP_H
#define KEYMAP_H

// HID 키코드(KEY_A 등)는 USB Type에 Keyboard가 포함돼야 정의된다.
#if !defined(KEY_A)
  #error "Tools > USB Type 을 'Serial + Keyboard + Mouse + Joystick' 으로 설정하세요."
#endif

// side: 이 키를 담당하는 손. 6/T/G/B까지가 왼쪽, 7/Y/H/N부터가 오른쪽.
#define SIDE_L      0
#define SIDE_R      1
#define SIDE_SPLIT  2   // 키 중심을 경계로 누른 위치에 따라 좌/우가 갈린다 (SPACE)

// code: 버클링 시 PC로 보낼 HID 키코드.
//   KEY_*        일반 키       -> 리포트의 6키 슬롯에 들어간다
//   MODIFIERKEY_*  모디파이어  -> 리포트의 modifier 바이트에 OR된다 (누르는 동안 유지)
//   0            HID 출력 없음 (햅틱만)
struct Key { const char* label; float cx, cy, hx, hy; byte side; uint16_t code; };

float CAL_OX = -18.8f;
float CAL_OY = 23.0f;

#define KEY_GAIN_MIN 0.1f   // 엣지에서의 최소 세기(중심=1.0)

// 어떤 키캡에도 없는 접촉(갭)의 좌/우는 이 x 경계로 대신 판정한다.
#define X_SPLIT_FALLBACK 108.0f

static const Key KEYS[] = {
  {"1",  28.7,  9.4, 8.1, 8.1, SIDE_L, KEY_1}, {"2",  47.8,  9.4, 8.1, 8.1, SIDE_L, KEY_2},
  {"3",  66.9,  9.4, 8.1, 8.1, SIDE_L, KEY_3}, {"4",  86.0,  9.4, 8.1, 8.1, SIDE_L, KEY_4},
  {"5", 105.1,  9.4, 8.1, 8.1, SIDE_L, KEY_5}, {"6", 124.2,  9.4, 8.1, 8.1, SIDE_L, KEY_6},
  {"7", 143.2,  9.4, 8.1, 8.1, SIDE_R, KEY_7}, {"8", 162.4,  9.4, 8.1, 8.1, SIDE_R, KEY_8},
  {"9", 181.5,  9.4, 8.1, 8.1, SIDE_R, KEY_9}, {"0", 200.6,  9.4, 8.1, 8.1, SIDE_R, KEY_0},
  {"BACKSPACE", 229.3, 9.4, 18.2, 8.1, SIDE_R, KEY_BACKSPACE},   // -,+ 두 키 위치를 합쳐 backspace로 인식
  {"Q",  38.2, 28.2, 8.1, 8.1, SIDE_L, KEY_Q}, {"W",  57.3, 28.2, 8.1, 8.1, SIDE_L, KEY_W},
  {"E",  76.4, 28.2, 8.1, 8.1, SIDE_L, KEY_E}, {"R",  95.5, 28.2, 8.1, 8.1, SIDE_L, KEY_R},
  {"T", 114.6, 28.2, 8.1, 8.1, SIDE_L, KEY_T}, {"Y", 133.7, 28.2, 8.1, 8.1, SIDE_R, KEY_Y},
  {"U", 152.8, 28.2, 8.1, 8.1, SIDE_R, KEY_U}, {"I", 171.9, 28.2, 8.1, 8.1, SIDE_R, KEY_I},
  {"O", 191.0, 28.2, 8.1, 8.1, SIDE_R, KEY_O}, {"P", 210.1, 28.2, 8.1, 8.1, SIDE_R, KEY_P},
  {"A",  43.0, 47.0, 8.1, 8.1, SIDE_L, KEY_A}, {"S",  62.1, 47.0, 8.1, 8.1, SIDE_L, KEY_S},
  {"D",  81.2, 47.0, 8.1, 8.1, SIDE_L, KEY_D}, {"F", 100.3, 47.0, 8.1, 8.1, SIDE_L, KEY_F},
  {"G", 119.4, 47.0, 8.1, 8.1, SIDE_L, KEY_G}, {"H", 138.5, 47.0, 8.1, 8.1, SIDE_R, KEY_H},
  {"J", 157.6, 47.0, 8.1, 8.1, SIDE_R, KEY_J}, {"K", 176.7, 47.0, 8.1, 8.1, SIDE_R, KEY_K},
  {"L", 195.8, 47.0, 8.1, 8.1, SIDE_R, KEY_L},
  {"Z",  52.5, 65.8, 8.1, 8.1, SIDE_L, KEY_Z}, {"X",  71.6, 65.8, 8.1, 8.1, SIDE_L, KEY_X},
  {"C",  90.7, 65.8, 8.1, 8.1, SIDE_L, KEY_C}, {"V", 109.8, 65.8, 8.1, 8.1, SIDE_L, KEY_V},
  {"B", 128.9, 65.8, 8.1, 8.1, SIDE_L, KEY_B}, {"N", 148.0, 65.8, 8.1, 8.1, SIDE_R, KEY_N},
  {"M", 167.1, 65.8, 8.1, 8.1, SIDE_R, KEY_M},
  {"SPACE", 130.7, 84.6, 59.7, 7.5, SIDE_SPLIT, KEY_SPACE},

  // ── 아직 없는 키 ──────────────────────────────────────────────────────────
  // QWERTY.pdf 오버레이에는 물리적으로 더 많은 키가 있다. 위 좌표를 역산하면
  // 표준 u-그리드(1u = 19.1mm, 각 행 15u = 286.5mm)와 정확히 맞는다:
  //   Q = 2u, A = 2.25u, Z = 2.75u, SPACE = 3.75~10u  <- 셋 다 실측값과 일치
  // 그래서 미매핑 키의 위치도 그리드로 복원할 수 있다.
  //
  // 제약은 Sensel Morph의 감지영역이 230 x 130mm뿐이라는 것 (240 x 139는 기기
  // 외형 치수다). CAL_OX = -18.8 이므로 여기 좌표계에서 센서는 x = 18.8 ~ 248.8.
  // 이 값이 맞다는 근거는 위 표 자체다: 쓰이는 x 범위가 20.6("1"의 왼쪽 모서리)
  // ~ 247.5(BACKSPACE의 오른쪽 모서리)라 좌우 여백이 1.8 / 1.3mm로 딱 떨어진다.
  //
  // 양 끝 열은 감지영역 밖으로 삐져나가 "키캡의 안쪽 일부만" 센서에 걸린다:
  //
  //   전폭 19.1mm 전부       : [ ; ' , . /          -> 그대로 추가 가능
  //   23.9mm 전부            : WIN, ALT (양쪽)      -> 그대로 추가 가능
  //   24.2mm / 43.0mm        : 왼쪽 SHIFT           -> 안쪽 절반만 누르면 됨
  //   14.9mm / 52.5mm        : 오른쪽 SHIFT         -> 안쪽만
  //   14.6mm / 33.4mm        : CAPS LOCK            -> 안쪽만
  //   10.1mm / 19.1mm        : ]                    -> 빠듯
  //   10.0mm / 23.9mm        : MENU                 -> 빠듯
  //    9.8mm / 28.6mm        : TAB                  -> 빠듯
  //    5.3mm / 43.0mm        : RETURN               -> 손가락 폭보다 좁다. 사실상 불가
  //    5.0mm / 23.9mm        : 왼쪽 CONTROL         -> 빠듯
  //   1mm 미만               : ` , \ , 오른쪽 CONTROL, 원래 위치의 BACKSPACE -> 불가
  //                            (BACKSPACE가 -,= 자리로 옮겨진 이유가 이것이다)
  //
  // 요약하면 SHIFT는 "키캡 안쪽을 누른다"는 제약을 받아들이면 양쪽 다 추가할 수
  // 있다. RETURN은 5.3mm라 현실적으로 어렵다 — 필요하면 다른 키에 재할당하거나
  // 오버레이를 오른쪽으로 밀고 CAL_OX를 다시 잡아야 한다.
  //
  // 아래는 그리드에서 나온 좌표. 끝 열은 센서에 걸리는 부분만 잡은 축소 히트박스다.
  // 실측 확인 후 주석을 풀 것.
  //
  //        {"[", 229.2, 28.2, 8.1, 8.1, SIDE_R, KEY_LEFT_BRACE},
  //        {";", 214.9, 47.0, 8.1, 8.1, SIDE_R, KEY_SEMICOLON},
  //        {"'", 234.0, 47.0, 8.1, 8.1, SIDE_R, KEY_QUOTE},
  //        {",", 186.2, 65.8, 8.1, 8.1, SIDE_R, KEY_COMMA},
  //        {".", 205.3, 65.8, 8.1, 8.1, SIDE_R, KEY_PERIOD},
  //        {"/", 224.4, 65.8, 8.1, 8.1, SIDE_R, KEY_SLASH},
  //        {"SHIFT",  30.9, 65.8, 12.1, 8.1, SIDE_L, MODIFIERKEY_SHIFT},  // 18.8~43.0
  //        {"SHIFT", 241.4, 65.8,  7.4, 8.1, SIDE_R, MODIFIERKEY_SHIFT},  // 233.9~248.8
  //        {"]",     243.8, 28.2,  5.0, 8.1, SIDE_R, KEY_RIGHT_BRACE},    // 238.8~248.8
  //
  // 주의: BACKSPACE가 -,= 자리(x 211.1~247.5)를 먹고 있어 위 몇몇과 x범위가
  // 겹친다. keyAt()은 배열 순서상 먼저 매치되는 키를 반환하므로 행(cy)이 다르면
  // 충돌하지 않지만, 새 키를 넣을 때마다 겹침을 확인할 것.
  //
  // 배치와 도달 가능 폭은 keyboard/test.html 하단 키보드 그림에서 눈으로 볼 수 있다.
  // ─────────────────────────────────────────────────────────────────────────
};
static const int N_KEYS = sizeof(KEYS)/sizeof(KEYS[0]);

// 접촉(sensel mm) -> 눌린 키 인덱스와 위치 gain 반환.
//   반환 -1: 어떤 키캡에도 없음(갭) -> 호출부에서 햅틱 0 처리
//   *gain: 0..1  (중심 1.0, 엣지 KEY_GAIN_MIN)
inline int keyAt(float sx, float sy, float* gain) {
  float lx = sx - CAL_OX;
  float ly = sy - CAL_OY;
  for (int i = 0; i < N_KEYS; i++) {
    float dx = lx - KEYS[i].cx;  if (dx < 0) dx = -dx;   // 중심에서의 |가로 거리|(mm)
    float dy = ly - KEYS[i].cy;  if (dy < 0) dy = -dy;   // 중심에서의 |세로 거리|(mm)
    if (dx < KEYS[i].hx && dy < KEYS[i].hy) {            // 키캡 안
      float m       = KEYS[i].hy;
      float plateau = KEYS[i].hx - m;     //넓은 키(SPACE/BACKSPACE) 가로 plateau 처리
      float ax = (dx > plateau) ? (dx - plateau) / m : 0.0f;
      float ay = dy / KEYS[i].hy;
      float r  = ax > ay ? ax : ay;
      *gain = KEY_GAIN_MIN + (1.0f - KEY_GAIN_MIN) * pow(1.0f - r, 2.0f);
      return i;
    }
  }
  *gain = 0.0f;
  return -1;
}

// 접촉이 어느 손 담당인지. ki는 keyAt()이 돌려준 키 인덱스, sx는 sensel x(mm).
//   키캡 위     -> 키에 지정된 side
//   SPACE       -> 키 중심 기준 왼쪽/오른쪽
//   키 밖(ki<0) -> x 경계로 대신 판정
inline bool keyIsLeft(int ki, float sx) {
  if (ki < 0)                        return sx < X_SPLIT_FALLBACK;
  if (KEYS[ki].side == SIDE_SPLIT)   return (sx - CAL_OX) < KEYS[ki].cx;
  return KEYS[ki].side == SIDE_L;
}

#endif
