#include <M5Unified.h>
#include <Wire.h>
#include <SD.h>

#define M5_I2C_ADDR 0x55
#define SDCARD_CSPIN GPIO_NUM_4

String curr_chord = "";
String next_chord = "";
volatile bool disp_update = false;
bool start_drumming = false; // 🥁 控制是否开始打鼓

static constexpr size_t buf_size = 1024;
uint8_t wav_buf[buf_size];

struct __attribute__((packed)) wav_header_t {
  char RIFF[4];
  uint32_t chunk_size;
  char WAVEfmt[8];
  uint32_t fmt_chunk_size;
  uint16_t audiofmt;
  uint16_t channel;
  uint32_t sample_rate;
  uint32_t byte_per_sec;
  uint16_t block_size;
  uint16_t bit_per_sample;
};

struct __attribute__((packed)) sub_chunk_t {
  char identifier[4];
  uint32_t chunk_size;
};

bool playSdWavForceShort(const char* filepath, int short_samples = 15000) {
  M5.Speaker.stop();  // 立刻停止之前的声音

  File file = SD.open(filepath);
  if (!file) {
    Serial.print("Failed to open: "); Serial.println(filepath);
    return false;
  }

  wav_header_t header;
  file.read((uint8_t*)&header, sizeof(header));
  if (memcmp(header.RIFF, "RIFF", 4) ||
      memcmp(header.WAVEfmt, "WAVEfmt ", 8) ||
      header.audiofmt != 1 ||
      header.bit_per_sample != 16) {
    file.close();
    Serial.println("Invalid WAV header.");
    return false;
  }

  file.seek(sizeof(wav_header_t));
  sub_chunk_t chunk;
  file.read((uint8_t*)&chunk, sizeof(chunk));
  while (memcmp(chunk.identifier, "data", 4) != 0) {
    file.seek(file.position() + chunk.chunk_size);
    file.read((uint8_t*)&chunk, sizeof(chunk));
  }

  int32_t data_len = chunk.chunk_size;
  bool is_stereo = header.channel > 1;
  int played_samples = 0;

  while (data_len > 0 && played_samples < short_samples) {
    size_t len = data_len < buf_size ? data_len : buf_size;
    len = file.read(wav_buf, len);
    data_len -= len;

    int samples_this_block = len / 2;
    if (played_samples + samples_this_block > short_samples) {
      samples_this_block = short_samples - played_samples;
    }

    M5.Speaker.playRaw((const int16_t*)wav_buf, samples_this_block, header.sample_rate, is_stereo, 1, 1);
    played_samples += samples_this_block;
  }

  file.close();
  return true;
}


void i2c_receive(int len) {
  char buf[33] = {0};
  int count = 0;
  while (Wire.available() && count < 32) {
    buf[count++] = Wire.read();
  }

  String msg = "";
  for (int i=0; i<count; i++) {
    if (isPrintable(buf[i])) msg += buf[i];
  }
  msg.trim();

  int sep = msg.indexOf(',');
  if (sep > 0) {
    curr_chord = msg.substring(0, sep);
    next_chord = msg.substring(sep+1);
    curr_chord.trim();
    next_chord.trim();

    start_drumming = true;  // ✅ 收到和弦后开始打鼓
  }

  disp_update = true;
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  M5.begin(cfg);
  Wire.begin(M5_I2C_ADDR);
  Wire.onReceive(i2c_receive);
  M5.Speaker.setVolume(255);

  Serial.begin(115200);
  M5.Display.setTextSize(2);
  M5.Display.println("Mounting SD...");
  if (!SD.begin(SDCARD_CSPIN, SPI, 25000000)) {
    M5.Display.println("SD mount failed!");
    while (true) delay(1000);
  }
  M5.Display.println("SD mounted.");
}

void loop() {
  M5.update();
  static uint32_t last_beat_time = 0;
  static int drum_step = 0;
  uint32_t now = millis();

  // 只有收到和弦后，才开始打鼓
  if (start_drumming && (now - last_beat_time > 500)) {
    last_beat_time = now;
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0,0);

    // ride 每拍
    M5.Display.println("Drum: /1.wav (ride)");
    Serial.println("Step:" + String(drum_step) + "Playing: /1.wav (ride)");
    playSdWavForceShort("/1.wav", 7000);

    // kick on 0,4
    if (drum_step == 0 || drum_step == 4) {
      M5.Display.println("Drum: /4.wav (kick)");
      Serial.println("Step:" + String(drum_step) + "Playing: /4.wav (kick)");
      playSdWavForceShort("/4.wav", 14000);
    }

    // snare on 2,6
    if (drum_step == 2 || drum_step == 6) {
      M5.Display.println("Drum: /2.wav (snare)");
      Serial.println("Step:" + String(drum_step) + "Playing: /2.wav (snare)");
      playSdWavForceShort("/2.wav", 14000);
    }

    // hihat on every odd
    if (drum_step % 2 == 1) {
      M5.Display.println("Drum: /3.wav (hihat)");
      Serial.println("Step:" + String(drum_step) + "Playing: /3.wav (hihat)");
      playSdWavForceShort("/3.wav", 14000);
    }

    drum_step = (drum_step + 1) % 8;
  }

  if (disp_update) {
    disp_update = false;
    M5.Display.setCursor(0,40);
    M5.Display.print("Curr: "); M5.Display.println(curr_chord);
    M5.Display.print("Next: "); M5.Display.println(next_chord);
  }
}
