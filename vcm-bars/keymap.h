#ifndef KEYMAP_H
#define KEYMAP_H

// side: 이 키를 담당하는 손. 6/T/G/B까지가 왼쪽, 7/Y/H/N부터가 오른쪽.
#define SIDE_L      0
#define SIDE_R      1
#define SIDE_SPLIT  2   // 키 중심을 경계로 누른 위치에 따라 좌/우가 갈린다 (SPACE)

struct Key { const char* label; float cx, cy, hx, hy; byte side; };

float CAL_OX = -18.8f;
float CAL_OY = 23.0f;

#define KEY_GAIN_MIN 0.1f   // 엣지에서의 최소 세기(중심=1.0)

// 어떤 키캡에도 없는 접촉(갭)의 좌/우는 이 x 경계로 대신 판정한다.
#define X_SPLIT_FALLBACK 108.0f

static const Key KEYS[] = {
  {"1",  28.7,  9.4, 8.1, 8.1, SIDE_L}, {"2",  47.8,  9.4, 8.1, 8.1, SIDE_L},
  {"3",  66.9,  9.4, 8.1, 8.1, SIDE_L}, {"4",  86.0,  9.4, 8.1, 8.1, SIDE_L},
  {"5", 105.1,  9.4, 8.1, 8.1, SIDE_L}, {"6", 124.2,  9.4, 8.1, 8.1, SIDE_L},
  {"7", 143.2,  9.4, 8.1, 8.1, SIDE_R}, {"8", 162.4,  9.4, 8.1, 8.1, SIDE_R},
  {"9", 181.5,  9.4, 8.1, 8.1, SIDE_R}, {"0", 200.6,  9.4, 8.1, 8.1, SIDE_R},
  {"BACKSPACE", 229.3, 9.4, 18.2, 8.1, SIDE_R},   // -,+ 두 키 위치를 합쳐 backspace로 인식
  {"Q",  38.2, 28.2, 8.1, 8.1, SIDE_L}, {"W",  57.3, 28.2, 8.1, 8.1, SIDE_L},
  {"E",  76.4, 28.2, 8.1, 8.1, SIDE_L}, {"R",  95.5, 28.2, 8.1, 8.1, SIDE_L},
  {"T", 114.6, 28.2, 8.1, 8.1, SIDE_L}, {"Y", 133.7, 28.2, 8.1, 8.1, SIDE_R},
  {"U", 152.8, 28.2, 8.1, 8.1, SIDE_R}, {"I", 171.9, 28.2, 8.1, 8.1, SIDE_R},
  {"O", 191.0, 28.2, 8.1, 8.1, SIDE_R}, {"P", 210.1, 28.2, 8.1, 8.1, SIDE_R},
  {"A",  43.0, 47.0, 8.1, 8.1, SIDE_L}, {"S",  62.1, 47.0, 8.1, 8.1, SIDE_L},
  {"D",  81.2, 47.0, 8.1, 8.1, SIDE_L}, {"F", 100.3, 47.0, 8.1, 8.1, SIDE_L},
  {"G", 119.4, 47.0, 8.1, 8.1, SIDE_L}, {"H", 138.5, 47.0, 8.1, 8.1, SIDE_R},
  {"J", 157.6, 47.0, 8.1, 8.1, SIDE_R}, {"K", 176.7, 47.0, 8.1, 8.1, SIDE_R},
  {"L", 195.8, 47.0, 8.1, 8.1, SIDE_R},
  {"Z",  52.5, 65.8, 8.1, 8.1, SIDE_L}, {"X",  71.6, 65.8, 8.1, 8.1, SIDE_L},
  {"C",  90.7, 65.8, 8.1, 8.1, SIDE_L}, {"V", 109.8, 65.8, 8.1, 8.1, SIDE_L},
  {"B", 128.9, 65.8, 8.1, 8.1, SIDE_L}, {"N", 148.0, 65.8, 8.1, 8.1, SIDE_R},
  {"M", 167.1, 65.8, 8.1, 8.1, SIDE_R},
  {"SPACE", 130.7, 84.6, 59.7, 7.5, SIDE_SPLIT},
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
