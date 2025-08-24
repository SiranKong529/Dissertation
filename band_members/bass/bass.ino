#include <M5Unified.h>
#include <Wire.h>
#include <SD.h>

#define M5_I2C_ADDR 0x55
#define SDCARD_CSPIN GPIO_NUM_4

String current_chord_str = "";
String next_chord_str = "";

char i2c_raw_buffer[33] = {0};
volatile bool has_new_raw_i2c_data = false;

int current_chord[4] = {0,0,0,0};
int next_chord[4] = {0,0,0,0};

static uint32_t last_note_time = 0;
static int play_index = 0;  
static bool play_next = false;

static constexpr size_t buf_size = 1024;
uint8_t wav_buf[buf_size];

struct __attribute__((packed)) WavHeader {
  char riff_id[4]; uint32_t riff_size;
  char wave_id[4];
  char fmt_id[4]; uint32_t fmt_size;
  uint16_t audio_format; uint16_t num_channels;
  uint32_t sample_rate; uint32_t byte_rate;
  uint16_t block_align; uint16_t bits_per_sample;
};
struct __attribute__((packed)) DataChunkHeader {
  char data_id[4]; uint32_t data_size;
};

// ------------------------ I2C 接收 -----------------------
void i2c_receive(int len) {
  char tmp[33]; int cnt=0;
  while(Wire.available() && cnt<32) tmp[cnt++]=Wire.read();
  if(cnt) {
    tmp[cnt]='\0';
    memcpy(i2c_raw_buffer, tmp, cnt+1);
    has_new_raw_i2c_data = true;
  }
}

// ------------------------ 解析和弦 -----------------------
void parseChord(const String &s, int *out) {
  int idx=0, last=0;
  for(int i=0;i<s.length()&&idx<4;i++){
    if(s.charAt(i)=='_') { out[idx++]=s.substring(last,i).toInt(); last=i+1; }
  }
  if(idx<4 && last<s.length()) out[idx++]=s.substring(last).toInt();
}

// ------------------------ 播放短 WAV -----------------------
bool playSdWavForceShort(const char* filepath, int short_samples = 10000) {
  M5.Speaker.stop();  // 立即停止上一个音
  File file = SD.open(filepath);
  if (!file) {
    Serial.print("Failed to open: "); Serial.println(filepath);
    return false;
  }

  WavHeader header;
  file.read((uint8_t*)&header, sizeof(header));
  if (strncmp(header.riff_id,"RIFF",4) || strncmp(header.wave_id,"WAVE",4) ||
      strncmp(header.fmt_id,"fmt ",4) || header.audio_format!=1) {
    Serial.println("Invalid WAV header."); file.close(); return false;
  }

  DataChunkHeader chunk;
  bool found=false;
  while(file.available()){
    file.read((uint8_t*)&chunk,sizeof(chunk));
    if(!strncmp(chunk.data_id,"data",4)){found=true;break;}
    file.seek(file.position()+chunk.data_size);
  }
  if(!found){file.close();return false;}

  int played_samples = 0;
  uint32_t data_len = chunk.data_size;
  uint16_t bytes_per_sample = header.bits_per_sample / 8;
  uint16_t bytes_per_frame = bytes_per_sample * header.num_channels;

  while (data_len > 0 && played_samples < short_samples) {
    size_t bytes_to_read = data_len < buf_size ? data_len : buf_size;
    size_t bytes_read = file.read(wav_buf, bytes_to_read);
    if (!bytes_read) break;

    int samples_this_block = bytes_read / bytes_per_frame;
    if (played_samples + samples_this_block > short_samples) {
      samples_this_block = short_samples - played_samples;
    }
    M5.Speaker.playRaw((int16_t*)wav_buf, samples_this_block, header.sample_rate, header.num_channels>1, 1,1);

    played_samples += samples_this_block;
    data_len -= bytes_read;
  }
  file.close();
  return true;
}

// ------------------------ SETUP -----------------------
void setup(){
  auto cfg = M5.config(); cfg.internal_spk=true; cfg.external_imu=false; cfg.internal_mic=false;
  M5.begin(cfg); M5.Speaker.setVolume(255);

  Wire.onReceive(i2c_receive);
  Wire.begin(M5_I2C_ADDR);
  Serial.begin(115200);

  M5.Display.clear(); M5.Display.setTextSize(2);
  M5.Display.println("Mounting SD...");
  if(!SD.begin(SDCARD_CSPIN,SPI,10000000)){
    M5.Display.println("SD mount failed!"); while(true) delay(1000);
  }
  M5.Display.println("SD mounted.");
}

// ------------------------ LOOP -----------------------
void loop(){
  M5.update();
  uint32_t now = millis();

  // 接收新的和弦
  if(has_new_raw_i2c_data){
    has_new_raw_i2c_data=false;
    String tmp(i2c_raw_buffer); tmp.trim();
    Serial.print("I2C raw: "); Serial.println(tmp);
    int sep = tmp.indexOf(',');
    if(sep>0){
      current_chord_str=tmp.substring(0,sep);
      next_chord_str=tmp.substring(sep+1);
      current_chord_str.trim(); next_chord_str.trim();
      parseChord(current_chord_str,current_chord);
      parseChord(next_chord_str,next_chord);
      Serial.println("New chords loaded.");
    }
  }

  // 持续循环播放
  if(now - last_note_time >= (play_index<=1?500:1000)){
    last_note_time=now;
    int *ch = play_next ? next_chord : current_chord;
    int note = 0;
    int short_samples = 16000;
    if (ch[0] == 0 && ch[1] == 0 && ch[2] == 0 && ch[3] == 0) {
      Serial.println("Skip empty chord [0 0 0 0]");
      play_index = -1;
      return;
    }


    switch(play_index){
      case 0: note = (ch[0]==0?1:ch[0]-1); short_samples=8000; break; // pre-root
      case 1: note = ch[0]; short_samples=8000; break; // root
      case 2: note = ch[1]; short_samples=16000; break; // 3rd
      case 3: note = ch[2]; short_samples=16000; break; // 5th
      case 4: note = ch[3]; short_samples=16000; break; // 7th
    }

    M5.Display.clear(); M5.Display.setCursor(0,0);
    M5.Display.printf("Note: %d", note);
    Serial.printf("Play note %d with short_samples=%d\n", note, short_samples);

    playSdWavForceShort((String("/") + note + ".wav").c_str(), short_samples);

    play_index++;
    if(play_index>4){
      if(!play_next){ play_next=true; play_index=0; }
      else { play_next=false; play_index=0; Serial.println("=== Loop again ==="); }
    }
  }
}
