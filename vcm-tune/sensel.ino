#include "sensel.h"
#include "sensel_register_map.h"

//RX buffer for Sensel serial
byte rx_buf[SENSEL_RX_BUFFER_SIZE];

//Counter for RX buffer
unsigned int counter = 0;

//마지막 senselGetFrame() 결과 코드 (SenselStatus). 실패 원인 진단용.
byte senselLastStatus = SENSEL_OK;

//Open Sensel device on SenselSerial
void senselOpen()
{
  SenselSerial.begin(115200);
  #ifdef SenselDebugSerial
    SenselDebugSerial.begin(115200);
  #endif
  delay(3000);
  #ifdef SenselDebugSerial
    SenselDebugSerial.println("Sensel Open Complete!");
  #endif
}

//Set frame content for Sensel device, supports SENSEL_REG_CONTACTS_FLAG
void senselSetFrameContent(byte content)
{
  senselWriteReg(SENSEL_REG_FRAME_CONTENT_CONTROL, 1, content);
}

//Set optional fields returned for each contact.
void senselSetContactsMask(byte mask)
{
  senselWriteReg(SENSEL_REG_CONTACTS_MASK, 1, mask);
}

//Start scanning on Sensel device
void senselStartScanning()
{
  senselWriteReg(SENSEL_REG_SCAN_ENABLED, 1, 1);
}

//Stop scanning on a Sensel device
void senselStopScanning()
{
  senselWriteReg(SENSEL_REG_SCAN_ENABLED, 1, 0);
}

//Read all available data from SenselSerial
void senselReadAvailable() {
  int len = SenselSerial.available();
  if (len > 0) {
    SenselSerial.readBytes(&rx_buf[counter%SENSEL_RX_BUFFER_SIZE], len);
    counter = (counter + len);
  }
}

//Write byte to register on Sensel device
void senselWriteReg(byte addr, byte sizeVar, byte data)
{
  SenselSerial.write(0x01);
  SenselSerial.write(addr);
  SenselSerial.write(sizeVar);
  SenselSerial.write(data);
  SenselSerial.write(data);
  SenselSerial.readBytes(rx_buf, 2);
  if (rx_buf[0] != SENSEL_PT_WRITE_ACK) {
    #ifdef SenselDebugSerial
      SenselDebugSerial.println("FAILED TO RECEIVE ACK ON WRITE");
    #endif
  }
}

//Read byte array from register on Sensel device
void senselReadReg(byte addr, byte sizeVar, byte* buf)
{
  byte checksum = 0;
  SenselSerial.write(0x81);
  SenselSerial.write(addr);
  SenselSerial.write(sizeVar);
  SenselSerial.readBytes(rx_buf, 4);
  if (rx_buf[0] != SENSEL_PT_READ_ACK) {
    #ifdef SenselDebugSerial
      SenselDebugSerial.println("FAILED TO RECEIVE ACK ON READ");
    #endif
    _senselFlush();
    return;
  }
  unsigned int resp_size = _convertBytesToU16(rx_buf[2], rx_buf[3]);
  SenselSerial.readBytes(buf, resp_size);
  SenselSerial.readBytes(&checksum, 1);
}

//Convert 4 bytes to a unsigned long
unsigned long _convertBytesToU32(byte b0, byte b1, byte b2, byte b3)
{
  return ((((unsigned long)b3) & 0xff) << 24) | ((((unsigned long)b2) & 0xff) << 16) | ((((unsigned long)b1) & 0xff) << 8) | (((unsigned long)b0) & 0xff);
}

//Convert 2 bytes to an unsigned int
unsigned int _convertBytesToU16(byte b0, byte b1)
{
  return ((((unsigned int)b1) & 0xff) << 8) | (((unsigned int)b0) & 0xff);
}

//Convert 2 bytes to a signed int
int _convertBytesToS16(byte b0, byte b1)
{
  return ((((int)b1)) << 8) | (((int)b0) & 0xff);
}

//Flush the SenselSerial of all data
void _senselFlush()
{
  while(SenselSerial.available() > 0) {
    SenselSerial.read();
    //GH: delete
    // delay(1);
  }
  SenselSerial.flush();
}

// 프레임 하나를 읽어 파싱한다.
//   반환 true : *frame이 유효한 새 프레임으로 갱신됨
//   반환 false: 타임아웃/깨진 패킷. *frame은 건드리지 않으므로
//               호출부가 직전 프레임을 유지할지 0으로 볼지 결정한다.
bool senselGetFrame(SenselFrame *frame)
{
  counter = 0;

  // 호스트가 기기에게 "나 준비됐으니 다음 프레임 데이터 넘겨" 명령 전송
  SenselSerial.write(0x81);
  SenselSerial.write(SENSEL_REG_SCAN_READ_FRAME);
  SenselSerial.write((byte)0x00);

  // 의미 없이 1ms씩 쉬던 delay(1) 대신, tight loop로 헤더 5바이트 대기
  unsigned long start_time = millis();
  while (SenselSerial.available() < 5) {
    if (millis() - start_time > SENSEL_HDR_TIMEOUT_MS) {
      senselLastStatus = SENSEL_ERR_HDR_TIMEOUT;
      _senselFlush();
      return false;
    }
  }

  // 헤더 5바이트 빠르게 읽기
  SenselSerial.readBytes(rx_buf, 5);

  if (rx_buf[0] != SENSEL_PT_RVS_ACK) {
    senselLastStatus = SENSEL_ERR_BAD_ACK;
    _senselFlush();
    return false;
  }

  // 페이로드 크기 계산
  unsigned int resp_size = _convertBytesToU16(rx_buf[3], rx_buf[4]);

  // 길이 필드 자체가 말이 안 되면(desync) 읽지 않고 버린다.
  if (resp_size < 8 || resp_size + 6 > SENSEL_RX_BUFFER_SIZE) {
    senselLastStatus = SENSEL_ERR_BAD_SIZE;
    _senselFlush();
    return false;
  }

  // 나머지 데이터 + 체크섬이 들어올 때까지 delay 없이 대기
  start_time = millis();
  while (SenselSerial.available() < (int)(resp_size + 1)) {
    if (millis() - start_time > SENSEL_PAYLOAD_TIMEOUT_MS) {
      senselLastStatus = SENSEL_ERR_PAYLOAD_TIMEOUT;
      _senselFlush();
      return false;
    }
  }

  // 나머지 데이터를 rx_buf의 5번 인덱스부터 이어서 싹 긁어옵니다.
  SenselSerial.readBytes(&rx_buf[5], resp_size + 1);

  if (rx_buf[5] != SENSEL_REG_CONTACTS_FLAG) {
    senselLastStatus = SENSEL_ERR_BAD_CONTENT;
    _senselFlush();
    return false;
  }

  const unsigned int contact_size = 16;
  unsigned int n = rx_buf[12];

  // 접촉 수와 패킷 길이가 맞지 않으면 desync된 프레임이다.
  // 이 검사가 없으면 깨진 n으로 contacts[] 밖(= 인접 전역변수)을 덮어써서 폭주한다.
  if (n * contact_size != resp_size - 8 || n > SENSEL_MAX_CONTACTS) {
    senselLastStatus = SENSEL_ERR_LEN_MISMATCH;
    _senselFlush();
    return false;
  }

  // 데이터 파싱 (순수 좌표 추출 로직은 그대로 유지)
  frame->n_contacts = n;
  for (unsigned int i = 0; i < n; i++) {
    unsigned int offset = 13 + i * contact_size;
    frame->contacts[i].id = rx_buf[offset+0];
    frame->contacts[i].type = rx_buf[offset+1];
    frame->contacts[i].x_pos = _convertBytesToU16(rx_buf[offset+2], rx_buf[offset+3])/256.0f;
    frame->contacts[i].y_pos = _convertBytesToU16(rx_buf[offset+4], rx_buf[offset+5])/256.0f;
    frame->contacts[i].total_force = _convertBytesToU16(rx_buf[offset+6], rx_buf[offset+7])/8.0f;
    frame->contacts[i].area = _convertBytesToU16(rx_buf[offset+8], rx_buf[offset+9])/1.0f;
    // Ellipse(orientation/major/minor)는 주석 처리하고 같은 6바이트에 Peak를 받는다.
    frame->contacts[i].peak_x = _convertBytesToU16(rx_buf[offset+10], rx_buf[offset+11])/256.0f;
    frame->contacts[i].peak_y = _convertBytesToU16(rx_buf[offset+12], rx_buf[offset+13])/256.0f;
    frame->contacts[i].peak_force = _convertBytesToU16(rx_buf[offset+14], rx_buf[offset+15])/8.0f;
  }

  senselLastStatus = SENSEL_OK;
  return true;
}

//Read contact frame data from SenselSerial.
void senselGetFrame_OLD(SenselFrame *frame)
{
  counter = 0;
  SenselSerial.write(0x81);
  SenselSerial.write(SENSEL_REG_SCAN_READ_FRAME);
  SenselSerial.write((byte)0x00);
  frame->n_contacts = 0;
  int i;
  int contact_size = 16;
  int timeout = 20;
  while(counter < 5 && timeout > 0){
    senselReadAvailable();
    delay(1);
    timeout--;
  }
  if(timeout == 0 || rx_buf[0] != SENSEL_PT_RVS_ACK){
    _senselFlush();
    return;
  }
  unsigned int resp_size = _convertBytesToU16(rx_buf[3], rx_buf[4]);
  timeout = 50;
  while(counter < resp_size+6 && timeout > 0){
    senselReadAvailable();
    delay(1);
    timeout--;
  }
  if(timeout == 0){
    _senselFlush();
    return;
  }
  frame->n_contacts = rx_buf[12];
  if(rx_buf[5] == SENSEL_REG_CONTACTS_FLAG && (unsigned int)(frame->n_contacts*contact_size) == resp_size-8){
    for(i = 0; i < frame->n_contacts; i++){
      int offset = 13+i*contact_size;
      frame->contacts[i].id = rx_buf[offset+0];
      frame->contacts[i].type = rx_buf[offset+1];
      frame->contacts[i].x_pos = _convertBytesToU16(rx_buf[offset+2],rx_buf[offset+3])/256.0f;
      frame->contacts[i].y_pos = _convertBytesToU16(rx_buf[offset+4],rx_buf[offset+5])/256.0f;
      frame->contacts[i].total_force = _convertBytesToU16(rx_buf[offset+6],rx_buf[offset+7])/8.0f;
      frame->contacts[i].area = _convertBytesToU16(rx_buf[offset+8],rx_buf[offset+9])/1.0f;
      // Ellipse 대신 Peak contact data (contact mask 0x08).
      frame->contacts[i].peak_x = _convertBytesToU16(rx_buf[offset+10],rx_buf[offset+11])/256.0f;
      frame->contacts[i].peak_y = _convertBytesToU16(rx_buf[offset+12],rx_buf[offset+13])/256.0f;
      frame->contacts[i].peak_force = _convertBytesToU16(rx_buf[offset+14],rx_buf[offset+15])/8.0f;
    }
  }
  else{
    _senselFlush();
  }
}

//Print SenselFrame contact information
void senselPrintFrame(SenselFrame *frame){
  #ifdef SenselDebugSerial
    SenselDebugSerial.print("Num Contacts: ");
    SenselDebugSerial.println(frame->n_contacts);
    for(int i = 0; i < frame->n_contacts; i++){
      SenselDebugSerial.print("Contact ");
      SenselDebugSerial.print(frame->contacts[i].id);
      SenselDebugSerial.print(": x_pos ");
      SenselDebugSerial.print(frame->contacts[i].x_pos);
      SenselDebugSerial.print(" y_pos ");
      SenselDebugSerial.print(frame->contacts[i].y_pos);
      SenselDebugSerial.print(" total_force ");
      SenselDebugSerial.print(frame->contacts[i].total_force);
      SenselDebugSerial.print(" peak_force ");
      SenselDebugSerial.println(frame->contacts[i].peak_force);
    }
  #endif
}
