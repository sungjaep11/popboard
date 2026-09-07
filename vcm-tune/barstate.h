#ifndef BARSTATE_H
#define BARSTATE_H

// 접촉(손가락) 하나의 버클링 상태. 같은 쪽에서 두 키가 겹쳐 눌리면
// 두 접촉이 각자 버클링해서 한 바에 클릭 두 번을 순차로 쌓는다.
struct ContactState {
  int   buttonState;   // 0: 버클링 전, 1: 버클링 후
  float prevForce;
  int   activeId;      // 이 슬롯이 담당 중인 접촉의 sensel contact id (-1: 없음)

  // 버클링(=키다운) 순간의 키 인덱스를 래치한다. -1: 키 없음.
  // 매 프레임 다시 계산하면 누른 채 손가락이 미끄러졌을 때 키가 바뀌어
  // 엉뚱한 키가 릴리즈되고 원래 키는 눌린 채 고착된다. 진짜 키보드처럼
  // "누른 순간의 키"를 뗄 때까지 유지해야 한다.
  int   heldKey;
  bool  heldByPeak;    // peak 경로로 눌렀으면 peak 히스테리시스로 해제

  // 버클링 순간의 키 중심으로부터의 거리(0~1)를 같이 래치한다. 임계힘 배수와
  // 변위 프리로드가 둘 다 여기서 나온다.
  // heldKey와 같은 이유다: 누른 채 엣지로 미끄러지면 임계가 올라가서 릴리즈
  // 임계(F_VALLEY x 배수)가 지금 주고 있는 힘보다 위로 가고, 손가락을 떼지도
  // 않았는데 키가 풀렸다 다시 눌린다. 한 번 버클링한 키는 뗄 때까지
  // "누른 순간의 뻣뻣함"을 유지해야 한다.
  float heldEdge;

  // ── 갭 장전 차단 (armed) ────────────────────────────────────────────────
  // 갭에서 쌓은 힘을 키에 통째로 넘기지 않는다. 갭 -> 키 진입 순간의
  // total_force를 entryForce로 잡고, 그 뒤 새로 증가한 힘만 버클링
  // 곡선에 넣는다. 따라서 손을 떼지 않고 기울인 뒤 더 누르면 정상
  // 증가량에서 버클링한다. 처음부터 키 위인 타건은 entryForce=0이다.
  bool  wasOnKey;
  float entryForce;
  float entryPeak;     // 갭 -> 키 진입 시 peak_force 기준점

  // 버클링한 뒤 뗄 때까지 이 접촉이 준 최대 힘. 릴리즈할 때 #rel 한 줄로 내보낸다.
  // #probe 가 찍는 f 는 임계를 막 넘긴 그 프레임의 힘이라 "얼마나 세게 내리쳤나"를
  // 말해주지 못한다 — 사람은 보통 클릭을 느낀 뒤에도 한참 더 눌러 넣는다.
  float maxForce;
  float maxPeak;       // 같은 누름에서의 최대 peak_force (#rel pfmax)

  // ── 미인식 타건 (#miss) ──────────────────────────────────────────────────
  // 버클링에 못 닿고 끝난 누름 하나를 여기 모은다. 두 갈래가 있다:
  //   갭   — keyAt()이 -1. 키캡 사이 빈 띠라 아무리 세게 눌러도 버클링이 막혀 있다.
  //   약함 — 키캡 위인데 힘이 그 자리의 임계(fPeak x forceScale)에 못 미쳤다.
  //          mode 2(vcm-force)에서 엣지 임계가 올라간 만큼 여기로 떨어진다.
  // 어느 쪽이든 #probe 도 #rel 도 안 나오므로 지금 로그에는 "아무 일도 없었다"로
  // 남는다. 실제로는 손가락이 그만큼 힘을 준 사건이고, mode 2 를 평가하려면
  // 바로 이 사건 수가 필요하다 — 걸러낸 게 자랑인지 손해인지가 여기서 갈린다.
  //
  // missMax : 이번 에피소드의 최대 원시 힘. 0 이면 에피소드가 안 열려 있다.
  // missAct : 그 프레임에 버클링 곡선이 실제로 본 힘. 갭에서 키로 들어온
  //           누름은 missMax에서 entryForce를 뺀 값이다.
  // miss*   : 그 최대 원시 힘이 나온 프레임의 위치. 사람이 겨냥한 자리에 제일 가깝다 —
  //           접촉은 누르는 내내 미끄러지므로 시작/끝 좌표는 덜 미덥다.
  //           missKey 는 갭이면 nearestKey()가 고른 기준 키이고, missOn 이
  //           그게 진짜 키캡 위였는지(약함) 갭이었는지를 가른다.
  // missArmed: 지금 에피소드를 열어도 되는가. 버클링하면 내려가고, 힘이 다시
  //           빠지면 올라간다. 이게 없으면 진짜 타건을 뗀 직후(힘이 fValley
  //           아래로 내려와 풀린 그 순간)가 곧바로 "버클링 못 한 누름"으로
  //           보여서, 정상 타건마다 #miss 가 하나씩 따라붙는다.
  float missMax, missAct;
  float missPeakMax;   // 버클링 실패 에피소드의 최대 peak_force
  float missArea;      // missMax가 나온 프레임의 접촉 면적
  float missEdge, missDx, missDy, missPx, missPy;
  int   missKey;
  bool  missOn;
  bool  missArmed;

  // ── mode 3 (beta) 진동 상태 ──────────────────────────────────────────────
  // 진동은 클릭 변위 위에 그냥 더해서 같은 바로 내보낸다. 클릭이 "눌렸다"를,
  // 진동이 "얼마나 빗겨 눌렀나"를 각각 나른다 — 두 신호가 같은 채널을 다투지
  // 않으므로 mode 1(진폭)처럼 클릭이 흐려지지도, mode 2(임계)처럼 키가 안
  // 눌리지도 않는다.
  //
  // vibT0  : 이벤트 버스트(누를 때 / 뗄 때)의 시작 시각(micros). 0 = 비활성.
  //          micros()가 정확히 0인 순간과 겹치지 않도록 발사할 때 최소 1로 만든다.
  // vibAmp : 그 버스트의 진폭(듀티). 발사 순간의 heldEdge로 정해 릴리즈까지 고정한다.
  //          heldKey/heldEdge와 같은 이유 — 누른 채 미끄러져도 세기가 안 변해야 한다.
  unsigned long vibT0;
  float vibAmp;

  // vibCont : vibWhen 3(상시)에서 지금 이 슬롯이 내야 할 진동 진폭(듀티).
  //           위 둘과 성격이 정반대다 — 버스트는 "발사 순간 값을 래치"하지만
  //           상시는 매 프레임 현재 위치로 다시 쓴다. 목적이 "누르지 않고
  //           어루만져서 모서리를 찾는 것"이라 손가락을 따라와야 하기 때문이다.
  //           접촉이 없으면 반드시 0이어야 한다 (아래 SideState 주석 참고).
  float vibCont;
  bool  vibContGap;

  // 키 틈 bump는 양의 클릭 방향과 반대인 음의 반사인 파형 한 번이다.
  // armed는 누른 채 계속 있을 때 반복 발사되지 않게 하는 히스테리시스다.
  bool          gapBumpArmed;
  unsigned long gapBumpT0;
  float         gapBumpAmp;
};

// bar 하나(한쪽 손)의 독립 상태. 좌/우가 각자 버클링 상태를 가져야
// 한쪽을 누르고 있어도 반대쪽이 정상적으로 클릭된다.
// 슬롯 c[0], c[1]은 같은 쪽에서 동시에 눌린 접촉 최대 2개를 각각 담당한다.
// 버클링 중에만 접촉 ID를 고정하고, 버클링 전과 릴리즈 후에는
// 힘이 더 큰 접촉이 슬롯을 가져간다. 레스팅 손가락이 슬롯을 독점하지 않게 한다.
//
// vibContS / vibGapContS : 키캡 / 틈 vibCont를 매끄럽게 따라가는 값.
//                           gapBump가 1/2이면 틈 진동 성분은 0이다.
//
// NOTE: Arduino IDE는 함수 프로토타입을 #include 직후에 자동 삽입하므로,
//       함수 인자로 쓰이는 타입은 .ino 본문이 아니라 헤더에 있어야 한다.
#define SIDE_SLOTS 2
struct SideState {
  ContactState c[SIDE_SLOTS];
  float vibContS;
  float vibGapContS;
};

#endif
