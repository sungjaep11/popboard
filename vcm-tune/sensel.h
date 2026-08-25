#ifndef SENSEL_H
#define SENSEL_H

// Serial for the Sensel device
// senselSerial is defined in the main ino file.
#define SenselSerial senselSerial

//Serial for debug information
#define SenselDebugSerial Serial

//Max size for Sensel RX buffer
// #define SENSEL_RX_BUFFER_SIZE 512
#define SENSEL_RX_BUFFER_SIZE 1024

//Struct for Sensel contact information
typedef struct __attribute__((__packed__))
{
    byte id;
    byte type;
    float x_pos;
    float y_pos;
    float total_force;
    float area;
    float orientation;
    float major_axis;
    float minor_axis;
} SenselContact;

//Max number of contacts a frame can hold
#define SENSEL_MAX_CONTACTS 16

// 응답 대기 타임아웃(ms).
// 이건 "얼마나 자주 읽을까"가 아니라 "기기가 죽었나"를 판단하는 값이다.
// 루프 주기(5ms)에 맞춰 짧게 잡으면, 접촉이 많아 기기가 프레임을 늦게 만들 때
// 곧 도착할 프레임을 버리고 스트림까지 desync시킨다. 기다리는 편이 항상 이득이다.
#define SENSEL_HDR_TIMEOUT_MS      20
#define SENSEL_PAYLOAD_TIMEOUT_MS  10

//senselGetFrame()이 왜 실패했는지 (진단용). senselLastStatus에 담긴다.
enum SenselStatus {
  SENSEL_OK = 0,
  SENSEL_ERR_HDR_TIMEOUT,      // 응답 헤더 5바이트가 제때 안 옴 (기기가 프레임을 아직 안 만듦)
  SENSEL_ERR_BAD_ACK,          // 헤더 첫 바이트가 RVS_ACK가 아님 (스트림 desync)
  SENSEL_ERR_BAD_SIZE,         // 길이 필드가 말이 안 됨
  SENSEL_ERR_PAYLOAD_TIMEOUT,  // 헤더는 왔는데 본문이 덜 옴
  SENSEL_ERR_BAD_CONTENT,      // content flag가 CONTACTS가 아님
  SENSEL_ERR_LEN_MISMATCH,     // 접촉 수와 패킷 길이가 안 맞음
  SENSEL_N_STATUS
};

extern byte senselLastStatus;

//Struct for Sensel frame, only contains contacts
typedef struct __attribute__((__packed__))
{
  byte n_contacts;
  SenselContact contacts[SENSEL_MAX_CONTACTS];
}SenselFrame;

//Flag for enabling contact scanning
const byte SENSEL_REG_CONTACTS_FLAG = 0x04;

//Ack for read register
const byte SENSEL_PT_READ_ACK = 1;

//Ack for read variable size register
const byte SENSEL_PT_RVS_ACK = 3;

//Ack for write register
const byte SENSEL_PT_WRITE_ACK = 5;

const byte SENSEL_CONTACT_TYPE_INVALID = 0;
const byte SENSEL_CONTACT_TYPE_START = 1;
const byte SENSEL_CONTACT_TYPE_MOVE = 2;
const byte SENSEL_CONTACT_TYPE_END = 3;

#endif
