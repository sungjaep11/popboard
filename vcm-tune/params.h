#ifndef PARAMS_H
#define PARAMS_H

// 런타임 튜너블 한 벌. 원본 세 스케치의 #define과 1:1로 대응한다.
// 의미는 vcm-tune.ino 상단 주석과 원본 스케치 주석 그대로다.
//
// NOTE: barstate.h와 같은 이유로 이 구조체들은 .ino가 아니라 헤더에 있어야 한다.
//       Arduino IDE가 함수 프로토타입을 #include 직후에 자동 삽입하기 때문에,
//       함수 인자·반환형으로 쓰이는 타입이 .ino 본문에 있으면 프로토타입이
//       "아직 없는 타입"을 참조해 컴파일이 깨진다.
struct Params {
  uint32_t magic;
  int   mode;            // 0 equal(위치 무관) / 1 position / 2 force / 3 beta(진동)
  float fPeak, fValley, fEnd;
  float zPeak, zValley, zEnd;
  float clickScale;
  float deadband;
  float fEdgeScale, fEdgeCurve;
  float gainMin, ampCurve;
  // mode 3 (beta). 클릭 곡선은 mode 0과 똑같이 두고 진동만 위치에 따라 바꾼다.
  int   vibWhen;         // 0 누를 때 / 1 뗄 때 / 2 누를 때+뗄 때 (지속음은 없다)
  float vibCenter, vibEdge;  // 중심 / 키캡 경계에서의 진동 세기 (0~1000, 1000 = 듀티 1.0)
  float vibCurve;        // 그 사이 보간 곡선의 지수. fEdgeCurve와 같은 방향
  float vibFreq;         // Hz
  int   vibDur;          // 버스트 길이 (ms)
  int   vibDelay;        // 버클링/릴리즈 후 발사까지의 지연 (ms). 마스킹 회피용
  float calOX, calOY;
  int   loopPeriod;      // ms. 5 = 200Hz
  int   maxFrameDrops;
  int   hid;             // HID 타이핑 활성
  int   probe;           // 버클링마다 #probe 한 줄
  int   verbose;         // 매 루프 상태 한 줄 (200Hz. 켜면 다른 로그가 묻힌다)
  int   hidLog;          // 리포트가 바뀔 때마다 #hid 한 줄
  int   telHz;           // 라이브 텔레메트리 주기 (0 = 끔)
};

// set/get이 보는 표의 한 줄. f와 i 중 하나만 채운다 (f != nullptr 이면 float).
struct Tunable { const char* name; float* f; int* i; float lo, hi; };

// 슬롯 하나의 이번 프레임 상태. 텔레메트리와 로그가 같은 값을 본다.
struct SlotInfo { int key; float edgeRaw, force, peak, gain, vib; };

#endif
