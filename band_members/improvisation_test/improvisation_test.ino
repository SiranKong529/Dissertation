#include <Wire.h>
#include <stdlib.h>
#include "ircomm_i2c.h"

// 定义 IR 和 M5Stack 通信地址与节拍间隔
ir_mode_t ircomm_mode;
ir_msg_status_t msg_status;

#define M5_I2C_ADDR 0x55
#define CHORD_DURATION 8000       // 每对和弦持续 8 秒
#define NUM_MEMBERS 4             // 接收器数量
#define POST_ACK_DELAY 700


// 状态机定义：用于最初同步一次，之后按绝对时间运行
enum SyncState {
  WAITING_FOR_START_SYNC,  // 初始状态：向所有设备广播 "prepare_sync"
  WAITING_FOR_ACKS,        // 等待所有成员回应 "ACK"
  SENDING_SYNC_DATA,       // 发送当前和弦或 reharm 插入和弦对
  WAITING_FOR_READY,       // 等待所有成员回应 "READY"
  BROADCAST_START,         // 向所有成员广播 "start"，并设置绝对起始时间
  PROMOTE_VOTE,
  VOTE,                    // 接收来自成员的投票 "VOTE_YES" / "VOTE_NO"
  WAIT_FOR_REHARM          // 插入 reharm 进行中的两个和弦等待
};
SyncState sync_state = WAITING_FOR_START_SYNC;

// 同步状态追踪变量
bool ack_received[NUM_MEMBERS] = {false};
bool ready_received[NUM_MEMBERS] = {false};
bool vote_received[NUM_MEMBERS] = {false};
bool vote_yes[NUM_MEMBERS] = {false};
bool vote_result = false;
bool sync = false;

// 和弦与 reharm 状态
int key_root = 0;  // 主调 C = 0
String mode = "major";
int degree_sequence[] = {2, 6, 3, 5, 4, 1};
int sequence_len = 6;
int current_index = 0;
int vote_round = 0;
int total_pass_count = 0;
bool last_was_reharm = false;
bool is_currently_reharm = false;
bool pending_vote = false;
float pass_rate = 0.0;

// 时间相关变量
unsigned long performance_start_time = 0;
unsigned long chord_start_time = 0;
unsigned long sync_start_time = 0;
unsigned long state_enter_ts = 0;
unsigned long vote_start_time = 0;
unsigned long sync_cost_time = 0;
unsigned long chord_send_time = 0;
unsigned long chord_ack_time = 0;
unsigned long vote_loop_timer = 0;
bool toggle_send = true;

String chord1, chord2;
String chord_pair;

// 根据级数构建实际和弦音（返回字符串如 0_4_7_11）
String getChordFromDegree(int key_root, int degree, String mode) {
  int major_scale[] = {0, 2, 4, 5, 7, 9, 11};
  int minor_scale[] = {0, 2, 3, 5, 7, 8, 10};
  int chord_intervals[4] = {0};
  String quality;

  if (mode == "major") {
    switch (degree) {
      case 1: case 4: quality = "maj7"; break;
      case 2: case 3: case 6: quality = "m7"; break;
      case 5: quality = "7"; break;
      case 7: quality = "m7b5"; break;
      default: quality = "m7"; break;
    }
  } else {
    switch (degree) {
      case 1: quality = "m7"; break;
      case 2: quality = "m7b5"; break;
      case 3: quality = "maj7"; break;
      case 4: quality = "m7"; break;
      case 5: quality = "7"; break;
      case 6: quality = "maj7"; break;
      case 7: quality = "7"; break;
      default: quality = "m7"; break;
    }
  }

  int* scale = (mode == "major") ? major_scale : minor_scale;
  int scale_degree = degree - 1;
  if (scale_degree < 0 || scale_degree > 6) return "0_0_0_0";
  int chord_root = (key_root + scale[scale_degree]) % 12;

  if (quality == "maj7") {
    chord_intervals[0] = 0; chord_intervals[1] = 4; chord_intervals[2] = 7; chord_intervals[3] = 11;
  } else if (quality == "m7") {
    chord_intervals[0] = 0; chord_intervals[1] = 3; chord_intervals[2] = 7; chord_intervals[3] = 10;
  } else if (quality == "7") {
    chord_intervals[0] = 0; chord_intervals[1] = 4; chord_intervals[2] = 7; chord_intervals[3] = 10;
  } else if (quality == "m7b5") {
    chord_intervals[0] = 0; chord_intervals[1] = 3; chord_intervals[2] = 6; chord_intervals[3] = 10;
  } else if (quality == "dim7") {
    chord_intervals[0] = 0; chord_intervals[1] = 3; chord_intervals[2] = 6; chord_intervals[3] = 9;
  } else {
    return "0_0_0_0";
  }

  char buf[32];
  sprintf(buf, "%d_%d_%d_%d",
    (chord_root + chord_intervals[0]) % 12,
    (chord_root + chord_intervals[1]) % 12,
    (chord_root + chord_intervals[2]) % 12,
    (chord_root + chord_intervals[3]) % 12);
  return String(buf);
}

// 构建 reharm 插入和弦对（ii - V）
String getReharmProgression(int reharm_deg) {
  int ii_deg = (reharm_deg + 1) % 7; if (ii_deg == 0) ii_deg = 7;
  int V_deg  = (reharm_deg + 4) % 7; if (V_deg == 0)  V_deg = 7;
  return getChordFromDegree(key_root, ii_deg, mode) + "," + getChordFromDegree(key_root, V_deg, mode);
}

// 收集并统计投票信息
//void collectVoteMessages() {
  //for (int i = 0; i < NUM_MEMBERS; i++) {
    //int n_bytes = checkRxMsgReady(i);
    //if (n_bytes > 0) {
      //char buf[32] = {0}; int cnt = 0;
      //getIRMessage(i, n_bytes, buf, cnt);
      //String msg = String(buf).substring(0, cnt); msg.trim();
      //if (msg == "VOTE_YES") vote_yes[i] = true;
      //vote_received[i] = true;
   // }
  //}
  //int yes_count = 0;
  //for (int i = 0; i < NUM_MEMBERS; i++) {
    //if (vote_received[i] && vote_yes[i]) yes_count++;
  //}
  //yes_count += 1; // drummer 自己的一票
  //vote_result = (yes_count >= 3); // 多数投票通过
//}
void collectVoteMessages() {
  for (int i = 0; i < NUM_MEMBERS; i++) {
    int n_bytes = checkRxMsgReady(i);
    if (n_bytes > 0) {
      char buf[32] = {0}; int cnt = 0;
      getIRMessage(i, n_bytes, buf, cnt);
      String msg = String(buf).substring(0, cnt); msg.trim();

      if ((msg.startsWith("VOTE_YES") || msg.startsWith("VOTE_NO")) && msg.length() >= 8) {
        
        // 提取最后一个字符作为成员编号
        int member_id = msg.substring(msg.length() - 1).toInt();
        if (member_id >= 1 && member_id <= NUM_MEMBERS) {
          int idx = member_id - 1;

          if (!vote_received[idx]) {  // 尚未投票才记录
            vote_received[idx] = true;
            vote_yes[idx] = msg.startsWith("VOTE_YES");

            //Serial.print("Received vote from member ");
            //Serial.print(member_id);
            //Serial.print(": ");
            //Serial.println(vote_yes[idx] ? "YES" : "NO");
          }
        }
      }
    }
  }
  // 统计 YES 票
  int yes_count = 0;
  for (int i = 0; i < NUM_MEMBERS; i++) {
    if (vote_received[i] && vote_yes[i]) yes_count++;
  }

  //Serial.print("Received votes: ");
  //for (int i = 0; i < NUM_MEMBERS; i++) {
    //Serial.print(vote_received[i] ? "1" : "0");
    //}
  //Serial.println();


  yes_count += 1;  // drummer 自己的 YES 票
  vote_result = (yes_count >= 3);  // 多数票通过

  //Serial.print("Vote result: ");
  //Serial.println(vote_result ? "APPROVED (reharm)" : "REJECTED (basic)");
}

void processReadyMessages() {
  for (int i = 0; i < 4; i++) {
    int n_bytes = checkRxMsgReady(i);
    if (n_bytes > 0) {
      char buf[32] = {0}; int cnt = 0;
      getIRMessage(i, n_bytes, buf, cnt);
      String msg = String(buf).substring(0, cnt); msg.trim();
      //Serial.print("SENDER: RX"); Serial.print(i); Serial.print(" Got: \"");
      //Serial.print(msg); Serial.println("\"");

      for (int j = 0; j < NUM_MEMBERS; j++) {
        if (msg == "READY" + String(j+1)) {
          if (!ready_received[j]) {
            ready_received[j] = true;
            Serial.print("SENDER: Marked READY from receiver "); Serial.println(j+1);
            }
          //ready_received[j] = true;
          //Serial.print("SENDER: Marked READY from receiver "); Serial.println(j+1);
        }
      }
    }
  }
}

bool allReadyReceived() {
  for (int i = 0; i < NUM_MEMBERS; i++) {
    if (!ready_received[i]) return false;
  }
  return true;
}


// 初始化设置
void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(2000);
  ircomm_mode.mode = MODE_RESET_STATUS;
  Wire.beginTransmission(IRCOMM_IR_ADDR);
  Wire.write((uint8_t*)&ircomm_mode, 1);
  Wire.endTransmission();
  Serial.println("Finished Initialisation");
  sync_start_time = millis();
  total_pass_count = 0;
  delay(50);
}

// loop 逻辑将在此后续补全
void loop() {
  unsigned long now = millis();

  switch (sync_state) {

    case WAITING_FOR_START_SYNC:
      if (now - sync_start_time >= 8000) {
        Serial.println("\n--- New sync cycle ---");
        for (int i = 0; i < NUM_MEMBERS; i++) {
          ack_received[i] = false;
          ready_received[i] = false;
        }

        const char* prepare_msg = "prepare_sync";
        Wire.beginTransmission(IRCOMM_IR_ADDR);
        Wire.write((uint8_t*)prepare_msg, strlen(prepare_msg));
        Wire.endTransmission();
        Serial.println("SENDER: Sent prepare_sync.");
        sync_state = WAITING_FOR_ACKS;
        state_enter_ts = now;
      }
      break;

    case WAITING_FOR_ACKS: {
      processAckMessages();
      if (allAckReceived() && now - state_enter_ts >= POST_ACK_DELAY) {
        sync_start_time = now;
        sync_state = SENDING_SYNC_DATA;
      }
      //bool all_acked = true;
      //for (int i = 0; i < NUM_MEMBERS; i++) {
        //int n = checkRxMsgReady(i);
        //if (n > 0) {
          //char buf[32] = {0}; int cnt = 0;
          //getIRMessage(i, n, buf, cnt);
          //String msg = String(buf).substring(0, cnt); msg.trim();
          //if (msg.startsWith("ACK")) {
            //ack_received[i] = true;
            //Serial.print("ACK from ");
            //Serial.print(i);
            //Serial.println(" received");
          //}
        //}
        //if (!ack_received[i]) all_acked = false;
      //}
      //if (all_acked) {
        //sync_state = SENDING_SYNC_DATA;
      //}
      else if(now - sync_start_time >= 30000){
        sync_state = WAITING_FOR_START_SYNC; //超过三十秒没有同步，就再发一次prepare
      }
      break;
    }

    case SENDING_SYNC_DATA: {
      // 发送两个和弦（当前和下一个）
      if(is_currently_reharm == true){
        String reharm = getReharmProgression(degree_sequence[(current_index + 2) % sequence_len]);
        chord_pair = reharm; 
      }
      else{
        chord1 = getChordFromDegree(key_root, degree_sequence[current_index], mode);
        chord2 = getChordFromDegree(key_root, degree_sequence[(current_index + 1) % sequence_len], mode);
        Serial.print(degree_sequence[current_index]);
        Serial.print(", ");
        Serial.println(degree_sequence[(current_index + 1) % sequence_len]);
        chord_pair = chord1 + "," + chord2 + "T";
      }

      char msg_for_ir[64];
      chord_pair.toCharArray(msg_for_ir, sizeof(msg_for_ir));
      //sprintf(msg_for_ir, chord_pair);
      chord_send_time = millis();
      while (millis() - chord_send_time < 50) {  // 持续发50ms
        Wire.beginTransmission(IRCOMM_IR_ADDR);
        Wire.write((uint8_t*)msg_for_ir, strlen(msg_for_ir));
        Wire.endTransmission();
        delay(3);
        }
      //Wire.beginTransmission(IRCOMM_IR_ADDR);
      //Wire.write((uint8_t*)msg_for_ir, strlen(msg_for_ir));
      //Wire.write((uint8_t*)chord_pair.c_str(), chord_pair.length());
      //Wire.endTransmission();
      Serial.print("Sent chord pair: "); Serial.println(msg_for_ir);
      
      sync_state = WAITING_FOR_READY;
      break;
    }

    case WAITING_FOR_READY: {
      processReadyMessages();

      if (allReadyReceived()) {
        chord_ack_time = now - chord_send_time;
        Serial.print("Chord Sync time: ");
        Serial.println(chord_ack_time);
        if(sync == false){
          Serial.println("SENDER: All READY received, broadcasting start.");
          const char* start_msg = "start";
          //for(int i = 0; i<100; i++){
            //Wire.beginTransmission(IRCOMM_IR_ADDR);
            //Wire.write((uint8_t*)start_msg, strlen(start_msg));
            //Wire.write((uint8_t*)"start", 5);
            //Wire.endTransmission();
          //}
          unsigned long start_broadcast_start = millis();
          while (millis() - start_broadcast_start < 100) {  // 持续发100ms
            Wire.beginTransmission(IRCOMM_IR_ADDR);
            Wire.write((uint8_t*)start_msg, strlen(start_msg));
            Wire.endTransmission();
            delay(5);
            }
          Serial.println("SENDER: Sent 'start' signal to all receivers.");
          sync = true;
          state_enter_ts = now;
          sync_cost_time = millis() - sync_start_time;
          Serial.print("System Sync time: ");
          Serial.println(sync_cost_time);
          sync_state = BROADCAST_START;
        }
        else{
          if(is_currently_reharm ==false){
            sync_state = PROMOTE_VOTE;
          }
          else{
            sync_state = WAIT_FOR_REHARM;
          }
        }
      }
      break;
    }

    case BROADCAST_START:
      if (millis() - state_enter_ts >= 5000) {
        performance_start_time = millis();
        chord_start_time = performance_start_time;
        Serial.println(">> SENDER: Initial start broadcast complete");
        char msg_for_m5[64];
        chord_pair.toCharArray(msg_for_m5, sizeof(msg_for_m5));
        //const char* msg_for_m5 = chord_pair;
        sendToM5(msg_for_m5);
        //Serial.print("Start first basic at: ");
        //Serial.println(millis());

        //for (int i = 0; i < NUM_MEMBERS; i++) {
          //vote_received[i] = false;
          //vote_yes[i] = false;
        //}
        //Wire.beginTransmission(IRCOMM_IR_ADDR);
        //Wire.write((uint8_t*)"start_vote", 10);
        //Wire.endTransmission();
        vote_start_time = millis();
        vote_round += 1;
        sync_state = VOTE;
      }
      break;

    case PROMOTE_VOTE:
      if (millis()-chord_start_time >= CHORD_DURATION){
        chord_start_time = millis();
        vote_start_time = millis();
        sendToM5(chord_pair);
        //Serial.print("Start basic at: ");
        //Serial.println(millis());
        //for (int i = 0; i < NUM_MEMBERS; i++) {
          //vote_received[i] = false;
          //vote_yes[i] = false;
        //}
        //Wire.beginTransmission(IRCOMM_IR_ADDR);
        //Wire.write((uint8_t*)"start_vote", 10);
        vote_round += 1;
        //Wire.endTransmission();
        sync_state = VOTE;
      }
      
      break;

    case VOTE:
      if (millis() - vote_start_time <= 5000){
        if(millis() - vote_loop_timer >= 50){
          vote_loop_timer = millis();

          if (toggle_send){
            Wire.beginTransmission(IRCOMM_IR_ADDR);
            Wire.write((uint8_t*)"start_vote", 10);
            Wire.endTransmission();
          }else{
            collectVoteMessages();
          }

          toggle_send = !toggle_send;  // 切换下一次发 or 收
        }
        //collectVoteMessages();
        //Wire.beginTransmission(IRCOMM_IR_ADDR);
        //Wire.write((uint8_t*)"start_vote", 10);
        //Wire.endTransmission();
        if(vote_received[0] && vote_received[1] && vote_received[2] && vote_received[3]){
          if (vote_result) {
            total_pass_count += 1;
            is_currently_reharm = true;
            Serial.println("vote result: reharm");
          } else {
            is_currently_reharm = false;
            Serial.println("vote result: basic");
          }
          current_index = (current_index + 2) % sequence_len;
          for (int i = 0; i < NUM_MEMBERS; i++) {
            vote_received[i] = false;
            vote_yes[i] = false;
          }
          pass_rate = float(total_pass_count)/(vote_round); //预期通过率是0.437
          Serial.print("pass rate: ");
          Serial.println(pass_rate);
          sync_state = SENDING_SYNC_DATA;
        }
      }
      else{
        pass_rate = float(total_pass_count)/(vote_round); //预期通过率是0.437
        Serial.print("pass rate: ");
        Serial.println(pass_rate);
        is_currently_reharm = false;
        current_index = (current_index + 2) % sequence_len;
        Serial.println("vote failed, send basic chord");
        for (int i = 0; i < NUM_MEMBERS; i++) {
          vote_received[i] = false;
          vote_yes[i] = false;
        }
        sync_state = SENDING_SYNC_DATA; 
      }
      //if (vote_received[0] && vote_received[1] && vote_received[2] && vote_received[3] && millis()-vote_start_time <= 5000) {
        //if (vote_result) {
          //is_currently_reharm = true;
          //Serial.println("vote result: reharm");
        //} else {
          //is_currently_reharm = false;
          //current_index = (current_index + 2) % sequence_len;
          //Serial.println("vote result: basic");
        //}
        //sync_state = SENDING_SYNC_DATA;
      //}
      //else{
        //is_currently_reharm = false;
        //current_index = (current_index + 2) % sequence_len;
        //Serial.println("vote failed, send basic chord");
        //sync_state = SENDING_SYNC_DATA; 
      //}
      break;

    case WAIT_FOR_REHARM:
      if (millis()-chord_start_time >= CHORD_DURATION) {
        Serial.print("Start reharm at: ");
        Serial.println(millis());
        chord_start_time = millis();
        sendToM5(chord_pair);
        is_currently_reharm = false;
        sync_state = SENDING_SYNC_DATA;
      }
      break;
  }
}

void sendToM5(String data) {
  Wire.beginTransmission(M5_I2C_ADDR);
  //Wire.write((uint8_t*)data, strlen(data));
  Wire.write(data.c_str(), data.length());
  Wire.endTransmission();
  //Serial.print("[M5] Sent chord: "); Serial.println(data);
  //Serial.print("SENDER: Uploaded to M5 drummer at "); Serial.println(millis());
  //Serial.println();
}

void processAckMessages() {
  for (int i = 0; i < 4; i++) {
    int n_bytes = checkRxMsgReady(i);
    if (n_bytes > 0) {
      char buf[32] = {0}; int cnt = 0;
      getIRMessage(i, n_bytes, buf, cnt);
      String msg = String(buf).substring(0, cnt); msg.trim();
      //Serial.print("SENDER: RX"); Serial.print(i); Serial.print(" Got: \"");
      //Serial.print(msg); Serial.println("\"");

      for (int j = 0; j < NUM_MEMBERS; j++) {
        if (msg == "ACK" + String(j+1)) {
          ack_received[j] = true;
          Serial.print("SENDER: Marked ACK from receiver "); Serial.println(j+1);
        }
      }
    }
  }
}

bool allAckReceived() {
  for (int i = 0; i < NUM_MEMBERS; i++) {
    if (!ack_received[i]) return false;
  }
  return true;
}

// ----------------- IR CHECK / RECEIVE -----------------
int checkRxMsgReady(int which_rx) {
  if (which_rx == 0) ircomm_mode.mode = MODE_SIZE_MSG0;
  else if (which_rx == 1) ircomm_mode.mode = MODE_SIZE_MSG1;
  else if (which_rx == 2) ircomm_mode.mode = MODE_SIZE_MSG2;
  else if (which_rx == 3) ircomm_mode.mode = MODE_SIZE_MSG3;
  else return 0;

  Wire.beginTransmission(IRCOMM_IR_ADDR);
  Wire.write((uint8_t*)&ircomm_mode, 1);
  Wire.endTransmission();
  Wire.requestFrom(IRCOMM_IR_ADDR, sizeof(msg_status));
  Wire.readBytes((uint8_t*)&msg_status, sizeof(msg_status));
  return msg_status.n_bytes;
}

void getIRMessage(int which_rx, int n_bytes, char* buf, int &count) {
  if (which_rx == 0) ircomm_mode.mode = MODE_REPORT_MSG0;
  else if (which_rx == 1) ircomm_mode.mode = MODE_REPORT_MSG1;
  else if (which_rx == 2) ircomm_mode.mode = MODE_REPORT_MSG2;
  else if (which_rx == 3) ircomm_mode.mode = MODE_REPORT_MSG3;

  Wire.beginTransmission(IRCOMM_IR_ADDR);
  Wire.write((uint8_t*)&ircomm_mode, 1);
  Wire.endTransmission();

  count = 0;
  Wire.requestFrom(IRCOMM_IR_ADDR, n_bytes);
  while (Wire.available() && count < 31) buf[count++] = Wire.read();
  buf[count] = '\0';
}