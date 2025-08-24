#include <Wire.h>
#include "ircomm_i2c.h"

//#define IRCOMM_IR_ADDR 0x40  // 修改为你的 IR 板 I2C 地址
#define M5_I2C_ADDR     0x55
#define CHORD_DURATION  8000
#define MEMBER_ID      2    // 本receiver的编号，从1开始（用于发送ACK2, READY2）
#define CHECK_INTERVAL 50

ir_mode_t ircomm_mode;
ir_msg_status_t msg_status;

enum State {
  WAIT_PREPARE_SYNC,
  WAIT_CHORD_SIGNAL,
  WAIT_PLAY_CHORD,
  VOTING_WINDOW,
  WAIT_FOR_REHARM
};
State state = WAIT_PREPARE_SYNC;

String current_chord = "";
String next_chord = "";

unsigned long target_start_time = 0;
unsigned long play_start_time = 0;
unsigned long check_message_ts = 0;
unsigned long vote_start_time = 0;
unsigned long expected_time = 0;
unsigned long former_playback_drift = 0;
unsigned long current_playback_drift = 0;
unsigned long playback_drift_diff = 0;

int chord_round = 0;                // 当前第几轮和弦


bool has_voted = false;
bool has_sync = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(2000);

  ircomm_mode.mode = MODE_RESET_STATUS;
  Wire.beginTransmission(IRCOMM_IR_ADDR);
  Wire.write((uint8_t*)&ircomm_mode, 1);
  Wire.endTransmission();
  delay(50);

  Serial.println("=== RECEIVER START ===");
}

void loop() {
  unsigned long now = millis();

  if (now - check_message_ts > CHECK_INTERVAL) {
    check_message_ts = now;
    switch (state) {
      case WAIT_PREPARE_SYNC:
        listenForPrepare();
        break;

      case WAIT_CHORD_SIGNAL:
        listenForChordPair();  // 含T标记
        break;

      case WAIT_PLAY_CHORD:
        if ((now >= target_start_time && !has_sync) ||
          (has_sync && now - play_start_time >= CHORD_DURATION)) {
          sendToM5(current_chord + "," + next_chord);

          current_playback_drift = now - expected_time;  // 可能为负或正
          playback_drift_diff = current_playback_drift - former_playback_drift;
          former_playback_drift = current_playback_drift;

          expected_time += 8000;
          Serial.print("[Drift] Round "); Serial.print(chord_round);
          Serial.print(": Drift = "); Serial.print(playback_drift_diff); Serial.println(" ms");
          chord_round += 1;

          play_start_time = now;
          has_voted = false;
          has_sync = true;
          state = VOTING_WINDOW;
          vote_start_time = now;
        }
        break;

      case VOTING_WINDOW:
        if(now - vote_start_time <= 5000){
          checkIRMessage([](String msg) {
            if (msg == "start_vote" && !has_voted) {
              sendVote();
              has_voted = true;
              state = WAIT_CHORD_SIGNAL; 
            }
          });
        }
        else{
          Serial.println("vote failed");
          Serial.println(current_chord + " & " + next_chord);
          state = WAIT_CHORD_SIGNAL; 
        }
        
        break;

      case WAIT_FOR_REHARM:
        if (now - play_start_time >= CHORD_DURATION) {
          sendToM5(current_chord + "," + next_chord);
          
          current_playback_drift = now - expected_time;  // 可能为负或正
          playback_drift_diff = current_playback_drift - former_playback_drift;
          former_playback_drift = current_playback_drift;

          expected_time += 8000;
          Serial.print("[Drift] Round "); Serial.print(chord_round);
          Serial.print(": Drift = "); Serial.print(playback_drift_diff); Serial.println(" ms");
          chord_round += 1;

          play_start_time = now;
          has_voted = false;
          state = WAIT_CHORD_SIGNAL;
        }
        break;
    }
  }
}

// ---------- IR Message Handlers ----------

void listenForPrepare() {
  checkIRMessage([](String msg) {
    if (msg.startsWith("ACK") || msg.startsWith("READY") || msg == "start" || msg.indexOf('T') != -1) {
        // 不打印，也不进入 parseBuffer
        return;
    }
    if (msg == "prepare_sync") {
      Serial.println("[Receiver] Got prepare_sync. Sending ACK.");
      sendIR("ACK" + String(MEMBER_ID));
      state = WAIT_CHORD_SIGNAL;
    }
  });
}

void listenForChordPair() {
  checkIRMessage([](String msg) {
    if (!has_sync) {
      if (msg == "start") {
        target_start_time = millis() + 5000;
        expected_time = target_start_time;
        Serial.println("[Receiver] Got start. Playing in 5s.");
        state = WAIT_PLAY_CHORD;
      }
      else if (msg.indexOf(',') != -1) {
        int sep = msg.indexOf(',');
        String chord1 = msg.substring(0, sep);
        String chord2 = msg.substring(sep + 1);
        chord2.trim();
        chord2.remove(chord2.length() - 1);  // 移除T标记
        current_chord = chord1;
        next_chord = chord2;
        sendIR("READY" + String(MEMBER_ID));
        Serial.print("[Receiver] Got first chord pair: ");
        Serial.println(current_chord + " & " + next_chord);
      }
    } else {
      if (msg.startsWith("ACK") || msg.startsWith("READY") || msg == "start" || msg == "prepare_sync") {
        return;
      }
      if (msg.indexOf(',') != -1 && msg != current_chord + "," + next_chord && msg != current_chord + "," + next_chord + "T") {
        int sep = msg.indexOf(',');
        String chord1 = msg.substring(0, sep);
        String chord2 = msg.substring(sep + 1);
        chord2.trim();
        if (chord2.endsWith("T")) {
          // 正常主和弦对
          chord2.remove(chord2.length() - 1);  // 移除T标记
          current_chord = chord1;
          next_chord = chord2;
          sendIR("READY" + String(MEMBER_ID));
          Serial.print("[Receiver] Next basic chord pair: ");
          Serial.println(current_chord + " & " + next_chord);
          state = WAIT_PLAY_CHORD;
        } else {
          if(MEMBER_ID == 1 || MEMBER_ID == 4){ //钢琴和萨克斯进行三全音替代
            current_chord = chord1;
            next_chord = tritone(chord2);
            Serial.println("perform tritone substitution");
          }
          else{
            current_chord = chord1;
            next_chord = chord2;
          }
          sendIR("READY" + String(MEMBER_ID));
          Serial.print("[Receiver] Playing reharm next: ");
          Serial.println(current_chord + " & " + next_chord);
          state = WAIT_FOR_REHARM;
        }
      }
    }
  });
}
String tritone(String chord_str) {
  // 解析和弦字符串（假设格式为: "7_11_2_5"）
  int notes[4];
  int idx = 0;
  int last_pos = 0;

  for (int i = 0; i < chord_str.length(); i++) {
    if (chord_str[i] == '_' || i == chord_str.length() - 1) {
      String num_str = chord_str.substring(last_pos, i == chord_str.length() - 1 ? i + 1 : i);
      notes[idx++] = num_str.toInt();
      last_pos = i + 1;
      if (idx >= 4) break;
    }
  }

  // 三全音替代：替换 root 为 root + 6（mod 12）
  int new_root = (notes[0] + 6) % 12;
  int interval1 = (notes[1] - notes[0] + 12) % 12;
  int interval2 = (notes[2] - notes[0] + 12) % 12;
  int interval3 = (notes[3] - notes[0] + 12) % 12;

  int n1 = (new_root + interval1) % 12;
  int n2 = (new_root + interval2) % 12;
  int n3 = (new_root + interval3) % 12;

  char buf[32];
  sprintf(buf, "%d_%d_%d_%d", new_root, n1, n2, n3);
  return String(buf);
}

// ---------- M5 & Voting ----------

void sendToM5(String data) {
  Wire.beginTransmission(M5_I2C_ADDR);
  Wire.write(data.c_str(), data.length());
  Wire.endTransmission();
  //Serial.print("[M5] Sent chord: "); Serial.println(data);
  Serial.print("🎵 Sent to M5 at: "); Serial.println(millis());
  Serial.println();
}

void sendVote() {
  bool vote_yes = (random(100) < 35);  // 可根据需要改为更复杂逻辑
  String vote_msg = String(vote_yes ? "VOTE_YES" : "VOTE_NO") + String(MEMBER_ID);
  sendIR(vote_msg);
  Serial.print("[Receiver] Sent vote: "); Serial.println(vote_msg);
  //Serial.print("In chord: "); Serial.println(current_chord + " & " + next_chord);
}

// --------------------
// 解析打印 buffer
String parseBuffer(char* buffer, int count) {
  String temp = "";
  //Serial.print("RAW: [");
  for (int j=0; j<count; j++) {
    //Serial.print((char)buffer[j]);
    if (isPrintable(buffer[j])) temp += buffer[j];
  }
  //Serial.println("]");
  temp.trim();
  //Serial.print("CLEAN: \""); Serial.print(temp); Serial.println("\"");
  return temp;
}
// ---------- Utility ----------

void checkIRMessage(void (*handler)(String)) {
  for (int i = 0; i < 4; i++) {
    int n_bytes = checkRxMsgReady(i);
    if (n_bytes > 0) {
      char buffer[64] = {0}; int count = 0;
      getIRMessage(i, n_bytes, buffer, count);
      String temp = parseBuffer(buffer, count);
      if (temp.length() > 0) {
        //Serial.print("[IR] RX"); Serial.print(i); Serial.print(": "); Serial.println(temp);
        handler(temp);
      }
    }
  }
}

//void sendIR(String msg) {
  //Wire.beginTransmission(IRCOMM_IR_ADDR);
  //Wire.write((uint8_t*)msg.c_str(), msg.length());
  //Wire.endTransmission();
  //delay(1);  // 添加一个微延时，降低 IR 拥堵
//}
void sendIR(String msg) {
  Wire.beginTransmission(IRCOMM_IR_ADDR);
  Wire.write((uint8_t*)msg.c_str(), msg.length());
  Wire.endTransmission();
}

// ---------- IR Lib Helpers ----------
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
  while (Wire.available() && count < 63) buf[count++] = Wire.read();
  buf[count] = '\0';
}
//int checkRxMsgReady(int which_rx) {
  //ircomm_mode.mode = MODE_SIZE_MSG0 + which_rx;
  //Wire.beginTransmission(IRCOMM_IR_ADDR);
  //Wire.write((uint8_t*)&ircomm_mode, 1);
  //Wire.endTransmission();
  //Wire.requestFrom(IRCOMM_IR_ADDR, sizeof(msg_status));
  //Wire.readBytes((uint8_t*)&msg_status, sizeof(msg_status));
  //return msg_status.n_bytes;
//}

//void getIRMessage(int which_rx, int n_bytes, char* buf, int &count) {
  //ircomm_mode.mode = MODE_REPORT_MSG0 + which_rx;
  //Wire.beginTransmission(IRCOMM_IR_ADDR);
  //Wire.write((uint8_t*)&ircomm_mode, 1);
  //Wire.endTransmission();

  //count = 0;
  //Wire.requestFrom(IRCOMM_IR_ADDR, n_bytes);
  //while (Wire.available() && count < 63) buf[count++] = Wire.read();
  //buf[count] = '\0';
//}
