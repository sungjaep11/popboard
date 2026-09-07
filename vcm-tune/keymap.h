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

// 어떤 키캡에도 없는 접촉(갭)의 좌/우는 이 x 경계로 대신 판정한다.
#define X_SPLIT_FALLBACK 108.0f

static const Key KEYS[] = {
  {"1",  28.7,  8.9, 8.5, 8.5, SIDE_L, KEY_1}, {"2",  47.8,  8.9, 8.5, 8.5, SIDE_L, KEY_2},
  {"3",  66.9,  8.9, 8.5, 8.5, SIDE_L, KEY_3}, {"4",  86.0,  8.9, 8.5, 8.5, SIDE_L, KEY_4},
  {"5", 105.1,  8.9, 8.5, 8.5, SIDE_L, KEY_5}, {"6", 124.2,  8.9, 8.5, 8.5, SIDE_L, KEY_6},
  {"7", 143.2,  8.9, 8.5, 8.5, SIDE_R, KEY_7}, {"8", 162.4,  8.9, 8.5, 8.5, SIDE_R, KEY_8},
  {"9", 181.5,  8.9, 8.5, 8.5, SIDE_R, KEY_9}, {"0", 200.6,  8.9, 8.5, 8.5, SIDE_R, KEY_0},
  {"BACKSPACE", 229.3, 8.9, 18.05, 8.5, SIDE_R, KEY_BACKSPACE},   // -,+ 두 키 위치를 합쳐 backspace로 인식
  {"Q",  38.2, 27.95, 8.5, 8.5, SIDE_L, KEY_Q}, {"W",  57.3, 27.95, 8.5, 8.5, SIDE_L, KEY_W},
  {"E",  76.4, 27.95, 8.5, 8.5, SIDE_L, KEY_E}, {"R",  95.5, 27.95, 8.5, 8.5, SIDE_L, KEY_R},
  {"T", 114.6, 27.95, 8.5, 8.5, SIDE_L, KEY_T}, {"Y", 133.7, 27.95, 8.5, 8.5, SIDE_R, KEY_Y},
  {"U", 152.8, 27.95, 8.5, 8.5, SIDE_R, KEY_U}, {"I", 171.9, 27.95, 8.5, 8.5, SIDE_R, KEY_I},
  {"O", 191.0, 27.95, 8.5, 8.5, SIDE_R, KEY_O}, {"P", 210.1, 27.95, 8.5, 8.5, SIDE_R, KEY_P},
  {"A",  43.0, 47.0, 8.5, 8.5, SIDE_L, KEY_A}, {"S",  62.1, 47.0, 8.5, 8.5, SIDE_L, KEY_S},
  {"D",  81.2, 47.0, 8.5, 8.5, SIDE_L, KEY_D}, {"F", 100.3, 47.0, 8.5, 8.5, SIDE_L, KEY_F},
  {"G", 119.4, 47.0, 8.5, 8.5, SIDE_L, KEY_G}, {"H", 138.5, 47.0, 8.5, 8.5, SIDE_R, KEY_H},
  {"J", 157.6, 47.0, 8.5, 8.5, SIDE_R, KEY_J}, {"K", 176.7, 47.0, 8.5, 8.5, SIDE_R, KEY_K},
  {"L", 195.8, 47.0, 8.5, 8.5, SIDE_R, KEY_L},
  {"Z",  52.5, 66.05, 8.5, 8.5, SIDE_L, KEY_Z}, {"X",  71.6, 66.05, 8.5, 8.5, SIDE_L, KEY_X},
  {"C",  90.7, 66.05, 8.5, 8.5, SIDE_L, KEY_C}, {"V", 109.8, 66.05, 8.5, 8.5, SIDE_L, KEY_V},
  {"B", 128.9, 66.05, 8.5, 8.5, SIDE_L, KEY_B}, {"N", 148.0, 66.05, 8.5, 8.5, SIDE_R, KEY_N},
  {"M", 167.1, 66.05, 8.5, 8.5, SIDE_R, KEY_M},
  {"SPACE", 130.7, 85.1, 58.65, 8.5, SIDE_SPLIT, KEY_SPACE},

  // ── 아직 없는 키 ──────────────────────────────────────────────────────────
  // QWERTY.pdf 오버레이에는 물리적으로 더 많은 키가 있다. 위 좌표를 역산하면
  // 표준 u-그리드(1u = 19.05mm = 0.75", 각 행 15u = 285.75mm)와 맞는다:
  //   Q = 2u, A = 2.25u, Z = 2.75u, SPACE = 3.75~10u  <- 셋 다 실측값과 일치
  // 그래서 미매핑 키의 위치도 그리드로 복원할 수 있다.
  //
  // 피치의 근거: QWERTY.pdf의 오버레이 이미지는 72dpi로 1:1 배치돼 있어 픽셀에서
  // 실치수를 역산할 수 있다. 키 외곽선 바깥변~바깥변 17.06mm, 인접 키 사이 갭
  // 2.01mm, 즉 피치는 가로 19.07 / 세로 18.98 로 가로세로 모두 19.05다.
  // cx는 19.1 간격으로 잡혀 있는데 열 끝("0","P")에서 0.45mm 벗어나는 정도라
  // 반폭 8.5mm 대비 무시할 수 있어 그대로 뒀다. cy는 18.8이라 4행 누적 1.0mm가
  // 쌓여서, 홈행(47.0)을 고정하고 19.05 피치로 다시 잡았다.
  //
  // 제약은 Sensel Morph의 감지영역이 230 x 130mm뿐이라는 것 (240 x 139는 기기
  // 외형 치수다). CAL_OX = -18.8 이므로 여기 좌표계에서 센서는 x = 18.8 ~ 248.8.
  // 이 값이 맞다는 근거는 위 표 자체다: 쓰이는 x 범위가 20.2("1"의 왼쪽 모서리)
  // ~ 247.35(BACKSPACE의 오른쪽 모서리)라 좌우 여백이 1.4 / 1.45mm로 딱 떨어진다.
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
  //        {"[", 229.2, 27.95, 8.5, 8.5, SIDE_R, KEY_LEFT_BRACE},
  //        {";", 214.9, 47.0, 8.5, 8.5, SIDE_R, KEY_SEMICOLON},
  //        {"'", 234.0, 47.0, 8.5, 8.5, SIDE_R, KEY_QUOTE},
  //        {",", 186.2, 66.05, 8.5, 8.5, SIDE_R, KEY_COMMA},
  //        {".", 205.3, 66.05, 8.5, 8.5, SIDE_R, KEY_PERIOD},
  //        {"/", 224.4, 66.05, 8.5, 8.5, SIDE_R, KEY_SLASH},
  //        {"SHIFT",  30.9, 66.05, 12.1, 8.5, SIDE_L, MODIFIERKEY_SHIFT},  // 18.8~43.0
  //        {"SHIFT", 241.4, 66.05,  7.4, 8.5, SIDE_R, MODIFIERKEY_SHIFT},  // 233.9~248.8
  //        {"]",     243.8, 27.95,  5.0, 8.5, SIDE_R, KEY_RIGHT_BRACE},    // 238.8~248.8
  //
  // 주의: BACKSPACE가 -,= 자리(x 211.25~247.35)를 먹고 있어 위 몇몇과 x범위가
  // 겹친다. keyAt()은 배열 순서상 먼저 매치되는 키를 반환하므로 행(cy)이 다르면
  // 충돌하지 않지만, 새 키를 넣을 때마다 겹침을 확인할 것.
  //
  // 배치와 도달 가능 폭은 keyboard/test.html 하단 키보드 그림에서 눈으로 볼 수 있다.
  // ─────────────────────────────────────────────────────────────────────────
};
static const int N_KEYS = sizeof(KEYS)/sizeof(KEYS[0]);

// 현재 KEYS[]에 등록된 키캡에 붙은 실제 틈을 고른다.
// 표준 키 피치(19.05mm) - 키캡 폭(17mm) = 약 2.05mm이므로,
// 활성 키 경계에서 2.1mm 안쪽이면 틈으로 본다. 활성 키가 두 개
// 가까워야 한다고 제한하면 행의 어괋난 끝, P–[/L–;/M–,과 같이
// 한쪽이 미사용 키인 실제 틈, SPACE 주변의 일부가 통째로 빠진다.
//
// 키보드 전체 외곽은 KEYS[]의 최소/최대 경계로 한 번 더 잘라
// 센서의 바깥 영역이 틈으로 울리지 않게 한다. 미사용 키캡 중심은
// 활성 키 경계에서 한참 멀어 그대로 무음이고, 사이의 2mm 틈만 울린다.
#define ACTIVE_GAP_HALO_MM 2.1f
inline bool activeKeyGapAt(float sx, float sy) {
  float lx = sx - CAL_OX;
  float ly = sy - CAL_OY;
  float minX = KEYS[0].cx - KEYS[0].hx;
  float maxX = KEYS[0].cx + KEYS[0].hx;
  float minY = KEYS[0].cy - KEYS[0].hy;
  float maxY = KEYS[0].cy + KEYS[0].hy;
  bool nearActiveKey = false;
  for (int i = 0; i < N_KEYS; i++) {
    float left = KEYS[i].cx - KEYS[i].hx;
    float right = KEYS[i].cx + KEYS[i].hx;
    float top = KEYS[i].cy - KEYS[i].hy;
    float bottom = KEYS[i].cy + KEYS[i].hy;
    if (left < minX) minX = left;
    if (right > maxX) maxX = right;
    if (top < minY) minY = top;
    if (bottom > maxY) maxY = bottom;

    float dx = lx - KEYS[i].cx; if (dx < 0.0f) dx = -dx;
    float dy = ly - KEYS[i].cy; if (dy < 0.0f) dy = -dy;
    if (dx < KEYS[i].hx + ACTIVE_GAP_HALO_MM &&
        dy < KEYS[i].hy + ACTIVE_GAP_HALO_MM) nearActiveKey = true;
  }
  return nearActiveKey && lx >= minX && lx <= maxX && ly >= minY && ly <= maxY;
}

// 접촉(sensel mm) -> 눌린 키 인덱스와 "중심에서 얼마나 벗어났나"를 반환.
//   반환 -1: 어떤 키캡에도 없음(갭). 클릭은 막고, 활성 키 사이 틈만 진동/bump를 낸다.
//   *edge: 0..1  (중심 0.0, 키캡 경계 1.0)
//   *offX, *offY: 부호 있는 오프셋(mm). +x는 오른쪽, +y는 아래(사용자 쪽).
//     edge 는 max(|offX|,|offY|) 꼴로 뭉갠 크기라 "어느 쪽으로" 빗나갔는지가
//     사라진다. 키별 계통 편향(= 키 중심을 옮겨서 없앨 수 있는 성분)과 시행
//     산포(= 못 없애는 성분)를 가르려면 부호가 있어야 하므로 따로 내보낸다.
//     갭(반환 -1)에서는 어느 키 기준인지가 없으므로 둘 다 0 — edge=1.0 과 같은
//     성격의 자리표시자다. 로그를 읽을 때 key="-" 인 줄은 offX/offY 를 버릴 것.
//
// keyboard/ 스케치는 여기서 곧바로 세기 gain(중심 1.0 -> 엣지 0.1)을 만들어
// 돌려줬다. 이 스케치는 같은 거리로 진폭과 임계힘 둘 다를 만들어야 하므로,
// 곡선을 씌우지 않은 정규화 거리 자체를 그대로 넘긴다. 곡선의 모양은
// vcm-tune.ino의 ampGain() / forceScale() 에서 각각 정하고, 둘 중 무엇을 켤지는
// 런타임 mode가 정한다 (0=진폭만 / 1=임계힘만 / 2=둘 다).
inline int keyAt(float sx, float sy, float* edge, float* offX, float* offY) {
  float lx = sx - CAL_OX;
  float ly = sy - CAL_OY;
  for (int i = 0; i < N_KEYS; i++) {
    float sx0 = lx - KEYS[i].cx;         // 부호 있는 가로 오프셋(mm). +는 오른쪽
    float sy0 = ly - KEYS[i].cy;         // 부호 있는 세로 오프셋(mm). +는 아래
    float dx = sx0 < 0 ? -sx0 : sx0;                     // 중심에서의 |가로 거리|(mm)
    float dy = sy0 < 0 ? -sy0 : sy0;                     // 중심에서의 |세로 거리|(mm)
    if (dx < KEYS[i].hx && dy < KEYS[i].hy) {            // 키캡 안
      float m       = KEYS[i].hy;
      float plateau = KEYS[i].hx - m;     //넓은 키(SPACE/BACKSPACE) 가로 plateau 처리
      float ax = (dx > plateau) ? (dx - plateau) / m : 0.0f;
      float ay = dy / KEYS[i].hy;
      *edge = ax > ay ? ax : ay;
      *offX = sx0;
      *offY = sy0;
      return i;
    }
  }
  // 갭은 "제일 먼 곳"이다. 여기서 0(=정중앙)을 돌려주면 키 사이 빈 띠가
  // 키 중심보다 무른 구간이 돼서, 엣지를 칠 때 접촉 중심이 넘어가는 순간
  // 임계가 F_PEAK로 떨어져 엉뚱하게 버클링한다.
  *edge = 1.0f;
  *offX = 0.0f;
  *offY = 0.0f;
  return -1;
}

// 키 ki 의 중심에서 (offX, offY) mm 떨어진 자리의 정규화 거리. keyAt() 이 키캡
// 안에서 쓰는 것과 같은 식인데 "키캡 안인가" 검사가 없다. 갭에서도 부르므로
// 1.0 을 넘을 수 있고, 그 초과분이 곧 "키캡을 얼마나 벗어났나"다 —
// 1.4 면 키 반높이의 1.4배, 즉 경계 밖으로 반높이의 0.4배만큼 나갔다는 뜻.
inline float keyEdgeOf(int ki, float offX, float offY) {
  float m       = KEYS[ki].hy;
  float plateau = KEYS[ki].hx - m;
  float dx = offX < 0 ? -offX : offX;
  float dy = offY < 0 ? -offY : offY;
  float ax = (dx > plateau) ? (dx - plateau) / m : 0.0f;
  float ay = dy / m;
  return ax > ay ? ax : ay;
}

// 갭(keyAt() == -1)에서 "제일 가까운 키"와 그 중심으로부터의 부호 있는 오프셋(mm).
// keyAt() 은 갭에서 offX/offY 를 0으로 돌려준다 — 기준 키가 없으니 당연하지만,
// 그러면 "어느 틈을 눌렀나"가 로그에서 통째로 사라진다. 미인식 타건(#miss)은
// 바로 그 틈이 궁금한 기록이라, 여기서 기준 키를 하나 정해 좌표를 붙여준다.
//
// 거리는 중심까지가 아니라 "키캡 밖으로 삐져나온 몫"으로 잰다. 키캡 크기가
// 제각각이라 중심 거리로 재면 SPACE 처럼 큰 키가 늘 이긴다 — Q 와 W 사이를
// 눌러도 SPACE 가 제일 가깝다고 나오면 아무 쓸모가 없다.
inline int nearestKey(float sx, float sy, float* offX, float* offY) {
  float lx = sx - CAL_OX;
  float ly = sy - CAL_OY;
  int   best = -1;
  float bestD = 0.0f;
  *offX = 0.0f; *offY = 0.0f;
  for (int i = 0; i < N_KEYS; i++) {
    float sx0 = lx - KEYS[i].cx;
    float sy0 = ly - KEYS[i].cy;
    float ox  = (sx0 < 0 ? -sx0 : sx0) - KEYS[i].hx;   // 키캡 밖으로 나간 가로 거리
    float oy  = (sy0 < 0 ? -sy0 : sy0) - KEYS[i].hy;   // 〃 세로
    if (ox < 0.0f) ox = 0.0f;
    if (oy < 0.0f) oy = 0.0f;
    float d = sqrtf(ox * ox + oy * oy);
    if (best < 0 || d < bestD) { best = i; bestD = d; *offX = sx0; *offY = sy0; }
  }
  return best;
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
