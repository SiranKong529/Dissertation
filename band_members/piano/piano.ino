#include <M5Unified.h>
#include <Wire.h>
#include <SD.h>
#include <string.h>

#define M5_I2C_ADDR 0x55
#define SDCARD_CSPIN GPIO_NUM_4

String current_chord = "";
String next_chord = "";
volatile bool has_new_chord = false;

char i2c_raw_buffer[33] = {0};
volatile int i2c_raw_len = 0;
volatile bool has_new_raw_i2c_data = false;

static constexpr size_t buf_size = 1024;
uint8_t wav_buf[buf_size];

struct __attribute__((packed)) WavHeader {
  char riff_id[4];
  uint32_t riff_size;
  char wave_id[4];
  char fmt_id[4];
  uint32_t fmt_size;
  uint16_t audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
};

struct __attribute__((packed)) DataChunkHeader {
  char data_id[4];
  uint32_t data_size;
};

void i2c_receive(int len) {
  char temp_buf[33] = {0};
  int count = 0;
  while (Wire.available() && count < 32) {
    temp_buf[count++] = Wire.read();
  }
  if (count > 0) {
    memcpy((void*)i2c_raw_buffer, temp_buf, count);
    i2c_raw_buffer[count] = '\0';
    i2c_raw_len = count;
    has_new_raw_i2c_data = true;
  }
}

bool playSdWav(const char* filepath, float seconds = 2.0) {
  M5.Speaker.stop();
  File file = SD.open(filepath);
  if (!file) {
    Serial.print("Failed to open WAV file: "); Serial.println(filepath);
    return false;
  }

  WavHeader header;
  if (file.read((uint8_t*)&header, sizeof(WavHeader)) != sizeof(WavHeader)) {
    file.close();
    return false;
  }

  if (strncmp(header.riff_id, "RIFF", 4) != 0 ||
      strncmp(header.wave_id, "WAVE", 4) != 0 ||
      strncmp(header.fmt_id, "fmt ", 4) != 0 ||
      header.audio_format != 1) {
    file.close();
    return false;
  }

  DataChunkHeader data_chunk;
  bool data_found = false;
  while (file.position() < file.size()) {
    if (file.read((uint8_t*)&data_chunk, sizeof(DataChunkHeader)) != sizeof(DataChunkHeader)) break;
    if (strncmp(data_chunk.data_id, "data", 4) == 0) {
      data_found = true;
      break;
    } else {
      file.seek(file.position() + data_chunk.data_size);
    }
  }

  if (!data_found) { file.close(); return false; }

  int samples_limit = seconds * header.sample_rate;
  int32_t data_len = data_chunk.data_size;
  uint16_t bytes_per_sample = header.bits_per_sample / 8;
  uint16_t bytes_per_frame = bytes_per_sample * header.num_channels;
  int played_samples = 0;

  while (data_len > 0 && played_samples < samples_limit) {
    size_t bytes_to_read = data_len < buf_size ? data_len : buf_size;
    size_t bytes_read = file.read(wav_buf, bytes_to_read);
    if (bytes_read == 0) break;

    int samples_this_block = bytes_read / bytes_per_frame;
    if (played_samples + samples_this_block > samples_limit) {
      samples_this_block = samples_limit - played_samples;
    }

    M5.Speaker.playRaw((const int16_t*)wav_buf, samples_this_block, header.sample_rate, header.num_channels > 1, 1, 1);
    played_samples += samples_this_block;
    data_len -= bytes_read;
  }

  file.close();
  return true;
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  cfg.external_imu = false;
  cfg.internal_mic = false;
  M5.begin(cfg);

  M5.Speaker.setVolume(255);

  Wire.begin(M5_I2C_ADDR);
  Wire.onReceive(i2c_receive);

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
  static uint32_t last_play_time = 0;
  static int play_count = 0; // 0,1 for current_chord, then 2,3 for next_chord
  uint32_t now = millis();

  if (has_new_raw_i2c_data) {
    has_new_raw_i2c_data = false;
    String temp_i2c_data = String(i2c_raw_buffer);
    temp_i2c_data.trim();
    Serial.print("I2C received raw in loop: "); Serial.println(temp_i2c_data);

    int sep = temp_i2c_data.indexOf(',');
    if (sep > 0) {
      current_chord = temp_i2c_data.substring(0, sep);
      next_chord = temp_i2c_data.substring(sep + 1);
      current_chord.trim();
      next_chord.trim();
      has_new_chord = true;
      play_count = 0; // reset cycle
      Serial.println("New chords parsed and ready:");
      Serial.print("   Current: "); Serial.println(current_chord);
      Serial.print("   Next:    "); Serial.println(next_chord);
    }
  }

  if (now - last_play_time > 2000) {
    String chord_to_play = "";
    if (play_count < 2) {
      chord_to_play = current_chord;
    } else if (play_count < 4) {
      chord_to_play = next_chord;
    }

    if (chord_to_play.length() > 0) {
      String filepath = "/" + chord_to_play + ".wav";
      M5.Display.clear(BLACK);
      M5.Display.setCursor(0,0);
      M5.Display.println("Piano:");
      M5.Display.println(filepath);

      Serial.print("Playing: "); Serial.println(filepath);
      if (!playSdWav(filepath.c_str(), 2.0)) {
        M5.Display.println("Play failed");
        Serial.println("Error: Playback failed for " + filepath);
      }
    }

    last_play_time = millis();
    play_count = (play_count + 1) % 4; // cycle back after 4 plays
  }
}
