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
};

// bar 하나(한쪽 손)의 독립 상태. 좌/우가 각자 버클링 상태를 가져야
// 한쪽을 누르고 있어도 반대쪽이 정상적으로 클릭된다.
// 슬롯 c[0], c[1]은 같은 쪽에서 동시에 눌린 접촉 최대 2개를 각각 담당한다.
//
// NOTE: Arduino IDE는 함수 프로토타입을 #include 직후에 자동 삽입하므로,
//       함수 인자로 쓰이는 타입은 .ino 본문이 아니라 헤더에 있어야 한다.
#define SIDE_SLOTS 2
struct SideState {
  ContactState c[SIDE_SLOTS];
};

#endif
