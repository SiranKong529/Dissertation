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
int solo_scale[7];

static constexpr size_t buf_size = 1024;
uint8_t wav_buf[buf_size];

uint32_t last_solo_note_time = 0;
uint32_t chord_start_time = 0;
bool chord_active = false;
bool play_next = false;

const uint32_t SOLO_NOTE_INTERVAL = 300;  // 修改为 300ms 一个音
const uint32_t SOLO_DURATION = 4000;

enum ChordType { CHORD_UNKNOWN, CHORD_MAJ7, CHORD_MIN7, CHORD_DOM7, CHORD_MINMAJ7, CHORD_HALF_DIM7, CHORD_DIM7 };
const char* chordTypeName[] = {"Unknown","Ionian","Dorian","Mixolydian","Melodic Minor","Locrian","Diminished"};

void i2c_receive(int len) {
    char tmp[33]; int cnt=0;
    while(Wire.available() && cnt<32) tmp[cnt++]=Wire.read();
    if(cnt){ tmp[cnt]='\0'; memcpy(i2c_raw_buffer,tmp,cnt+1); has_new_raw_i2c_data=true; }
}

void parseChord(const String &s, int *out) {
    int idx=0,last=0;
    for(int i=0;i<s.length()&&idx<4;i++)
      if(s.charAt(i)=='_'){out[idx++]=s.substring(last,i).toInt();last=i+1;}
    if(idx<4&&last<s.length())out[idx++]=s.substring(last).toInt();
}

ChordType determineChordType(int* n){
    int i3=(n[1]-n[0]+12)%12,i5=(n[2]-n[0]+12)%12,i7=(n[3]-n[0]+12)%12;
    if(i3==4&&i5==7&&i7==11) return CHORD_MAJ7;
    if(i3==4&&i5==7&&i7==10) return CHORD_DOM7;
    if(i3==3&&i5==7&&i7==10) return CHORD_MIN7;
    if(i3==3&&i5==7&&i7==11) return CHORD_MINMAJ7;
    if(i3==3&&i5==6&&i7==10) return CHORD_HALF_DIM7;
    if(i3==3&&i5==6&&i7==9) return CHORD_DIM7;
    return CHORD_UNKNOWN;
}

void calculateSoloScale(int root, ChordType type, int* out) {
    int pattern[7];
    switch(type){
        case CHORD_MAJ7: pattern[0]=0; pattern[1]=2; pattern[2]=4; pattern[3]=5; pattern[4]=7; pattern[5]=9; pattern[6]=11; break;
        case CHORD_MIN7: pattern[0]=0; pattern[1]=2; pattern[2]=3; pattern[3]=5; pattern[4]=7; pattern[5]=9; pattern[6]=10; break;
        case CHORD_DOM7: pattern[0]=0; pattern[1]=2; pattern[2]=4; pattern[3]=5; pattern[4]=7; pattern[5]=9; pattern[6]=10; break;
        case CHORD_MINMAJ7: pattern[0]=0; pattern[1]=2; pattern[2]=3; pattern[3]=5; pattern[4]=7; pattern[5]=9; pattern[6]=11; break;
        case CHORD_HALF_DIM7: pattern[0]=0; pattern[1]=1; pattern[2]=3; pattern[3]=5; pattern[4]=6; pattern[5]=8; pattern[6]=10; break;
        case CHORD_DIM7: pattern[0]=0; pattern[1]=2; pattern[2]=3; pattern[3]=5; pattern[4]=6; pattern[5]=8; pattern[6]=9; break;
        default: pattern[0]=0; pattern[1]=2; pattern[2]=4; pattern[3]=5; pattern[4]=7; pattern[5]=9; pattern[6]=11; break;
    }
    for(int i=0;i<7;i++) out[i]=(root+pattern[i])%12;
}

// 强行播放短
bool playSdWavForceShort(const char* filepath, int short_samples = 5000) {
    M5.Speaker.stop(); // 强行打断
    File f=SD.open(filepath); if(!f) return false;
    struct {char r[4]; uint32_t s; char w[4]; char fm[4]; uint32_t fs; uint16_t af,nc; uint32_t sr,br; uint16_t ba,bps;} h;
    if(f.read((uint8_t*)&h,sizeof(h))!=sizeof(h)){f.close();return false;}
    struct {char id[4]; uint32_t size;} dc;
    while(f.available()){ if(f.read((uint8_t*)&dc,sizeof(dc))!=sizeof(dc)) break;
      if(!strncmp(dc.id,"data",4)) break; f.seek(f.position()+dc.size);}
    int32_t data_len=dc.size;
    uint16_t bytes_per_sample = h.bps/8;
    uint16_t bytes_per_frame = bytes_per_sample * h.nc;
    int played_samples=0;
    while(data_len>0 && played_samples<short_samples){
      size_t bytes_to_read=data_len<buf_size?data_len:buf_size;
      size_t bytes_read=f.read(wav_buf,bytes_to_read);
      if(!bytes_read)break;
      int samples_this_block = bytes_read / bytes_per_frame;
      if(played_samples + samples_this_block > short_samples){
        samples_this_block = short_samples - played_samples;
      }
      M5.Speaker.playRaw((const int16_t*)wav_buf, samples_this_block, h.sr, h.nc>1,1,1);
      played_samples += samples_this_block;
      data_len -= bytes_read;
    }
    f.close(); return true;
}

void setup(){
    auto cfg=M5.config(); cfg.internal_spk=true; cfg.external_imu=false; cfg.internal_mic=false;
    M5.begin(cfg); M5.Speaker.setVolume(100);
    Wire.onReceive(i2c_receive); Wire.begin(M5_I2C_ADDR);
    Serial.begin(115200);
    M5.Display.setTextSize(2);
    if (!SD.begin(SDCARD_CSPIN, SPI, 25000000)) {
      M5.Display.println("SD mount failed!");
      while (true) delay(1000);
    }
    M5.Display.println("SD mounted.");
  }

void loop(){
    M5.update(); uint32_t now=millis();
    if(has_new_raw_i2c_data && !chord_active){
        has_new_raw_i2c_data=false;
        String tmp(i2c_raw_buffer); tmp.trim();
        int sep=tmp.indexOf(',');
        if(sep>0){
            current_chord_str=tmp.substring(0,sep); next_chord_str=tmp.substring(sep+1);
            parseChord(current_chord_str,current_chord);
            parseChord(next_chord_str,next_chord);
            chord_active=true; play_next=false;
            chord_start_time=now; last_solo_note_time=now;
        }
    }

    if(chord_active){
        if(now - chord_start_time > SOLO_DURATION){
            if(!play_next){
                play_next=true;
                chord_start_time=now;
                last_solo_note_time=now;
            } else {
                chord_active=false;
            }
        }

        if(now - last_solo_note_time > SOLO_NOTE_INTERVAL){
            last_solo_note_time=now;
            int* chord = play_next ? next_chord : current_chord;
            ChordType type = determineChordType(chord);
            calculateSoloScale(chord[0], type, solo_scale);
            int note=solo_scale[random(0,7)]+(random(0,2)?12:0);

            String mode = chordTypeName[type];
            String key = String("C C# D D# E F F# G G# A A# B").substring(chord[0]*2, chord[0]*2+2);
            M5.Display.clear(); M5.Display.setCursor(0,0);
            M5.Display.printf("%s %s",key.c_str(),mode.c_str());
            M5.Display.setCursor(0,30); M5.Display.printf("Note: %d", note);
            Serial.printf("%s %s Solo: %d.wav\n", key.c_str(), mode.c_str(), note);

            playSdWavForceShort(("/"+String(note)+".wav").c_str(), 8000); // 强行打断
        }
    }
}
