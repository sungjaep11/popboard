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
  // mode 3 (beta). 클릭 곡선은 mode 0과 똑같이 두고, 키 위는 편심 진동,
  // 키 틈은 gapBump 토글에 따라 상시 진동 또는 위쪽 bump를 낸다.
  int   vibWhen;         // 0 누를 때 / 1 뗄 때 / 2 누를 때+뗄 때 / 3 상시
                         // 0~2는 이벤트 버스트. 3만 버클링 없이 계속 나간다.
  float vibCenter, vibEdge;  // 데드밴드 끝점 / 키캡 경계에서의 진동 세기
                             // (0~1000, 1000 = 듀티 1.0). 데드밴드 안은 무음이다.
  float vibGap;          // gapBump=0일 때 활성 키캡 사이 틈의 진동 세기
  float vibCurve;        // 그 사이 보간 곡선의 지수. fEdgeCurve와 같은 방향
  float vibFreq;         // 키캡 위 진동 Hz
  float vibGapFreq;      // gapBump=0일 때 활성 키캡 사이 틈의 진동 Hz
  int   vibDur;          // 0~2: 버스트 길이 / 3: 마지막 움직임 뒤 유지 시간 (ms)
  int   vibDelay;        // 버클링/릴리즈 후 발사까지의 지연 (ms). 마스킹 회피용
  float calOX, calOY;
  int   loopPeriod;      // ms. 5 = 200Hz
  int   maxFrameDrops;
  int   hid;             // HID 타이핑 활성
  int   probe;           // 버클링마다 #probe 한 줄
  float missMin;         // 이 힘 이상 줬는데 버클링 못 한 누름마다 #miss 한 줄.
                         // 0 = 끔. 손을 얹어만 둔 접촉까지 다 잡히지 않게 하는 문턱이다.
  int   verbose;         // 매 루프 상태 한 줄 (200Hz. 켜면 다른 로그가 묻힌다)
  int   hidLog;          // 리포트가 바뀔 때마다 #hid 한 줄
  int   telHz;           // 라이브 텔레메트리 주기 (0 = 끔)
  // EEPROM VCM11 호환을 위해 끝에 추가한다. 상시 진동에서 접촉점이
  // 이만큼 움직이면 vibDur 정지 타이머를 다시 시작한다.
  float vibMove;
  // 키캡 사이 틈을 누를 때 나가는 단극성 위쪽 bump. 진동과 달리
  // 반복 왕복하지 않고, gapBumpForce를 넘는 순간 한 번만 발사한다.
  float gapBumpAmp;      // 세기 (0~1000, 1000 = 듀티 1.0)
  float gapBumpForce;    // 발사 문턱(센서 total_force)
  int   gapBumpDur;      // 위로 올라갔다 중립으로 돌아오는 시간(ms)
  int   gapBump;         // 0=틈 진동 / 1=bump 사용
  int   gapBumpMode;     // 0=기존 단발 bump / 1=힘 비례 상시 위쪽 변위
  float gapDisplaceForce;// gapBumpMode=1에서 gapBumpAmp에 도달하는 힘
  // 진동 발사 시점(vibWhen)과 독립적으로 세기에 수평 이동 속도를 곱한다.
  // 속도는 [vibSpeedMin, vibSpeedMax]를 0~1로 정규화한다.
  int   vibSpeedMode;    // 0=위치 세기만 / 1=위치 세기 × 이동 속도
  float vibSpeedMin;     // mm/s. 이하는 속도 배율 0
  float vibSpeedMax;     // mm/s. 이 속도에서 속도 배율 1
  float vibSpeedFloor;   // 0~1000. 정지 시에도 남겨둘 최소 속도 배율
  float vibPressFull;    // 한 프레임의 힘 변화가 이 값이면 누름/뗌 배율 1
  // VCM19: total_force 또는 peak_force 중 먼저 임계에 닿는 경로로 키다운.
  // Peak는 키다운에만 쓰고 릴리즈는 기존 total_force 곡선을 유지한다.
  int   peakEnable;
  float peakThreshold;   // 키 중심의 peak_force 임계(gf). mode 2는 forceScale 적용.
  float peakMinTotal;    // 단일 sensel 노이즈 차단용 최소 total_force(gf).
  // VCM20: 작은 접촉(실측 손톱)에서만 peak 보조 경로를 연다. 0=면적 게이트 끔.
  float peakAreaMax;
};

// set/get이 보는 표의 한 줄. f와 i 중 하나만 채운다 (f != nullptr 이면 float).
struct Tunable { const char* name; float* f; int* i; float lo, hi; };

// 슬롯 하나의 이번 프레임 상태. 텔레메트리와 로그가 같은 값을 본다.
struct SlotInfo {
  int key;
  float edgeRaw;
  float force;          // raw total_force
  float peak;           // total_force 키다운 임계(기존 필드명 유지)
  float gain, vib;
  float sensorPeak;     // raw peak_force
  float sensorArea;     // raw contact area
  float peakThreshold;  // 현재 위치의 peak_force 키다운 임계
  bool  peakHit;        // 이 프레임에 peak 경로가 임계를 넘었는지
};

#endif
