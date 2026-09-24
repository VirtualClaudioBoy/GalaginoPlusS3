#include "qix.h"

static_assert(sizeof(qix_main_rom) == 0x4000, "Unexpected Qix data ROM size");
static_assert(sizeof(qix_video_rom) == 0x3800, "Unexpected Qix video ROM size");
static_assert(sizeof(qix_sound_rom) == 0x0800, "Unexpected Qix sound ROM size");

qix *qix::active=nullptr;

uint16_t qix::pen(uint8_t v) const {
  // Qix palette is RRGGBBII.  The intensity bits feed all three guns.
  static const uint8_t level[16]={
    0x00,0x12,0x24,0x49, 0x12,0x24,0x49,0x92,
    0x5b,0x6d,0x92,0xdb, 0x7f,0x91,0xb6,0xff
  };
  uint8_t i=v&3, r=level[(((v>>6)&3)<<2)|i],
          g=level[(((v>>4)&3)<<2)|i], b=level[(((v>>2)&3)<<2)|i];
  uint16_t c=((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3);
  return (c>>8)|(c<<8);
}

void qix::start() {
  shared=memory; data_ram=memory+0x400; nvram=memory+0x800;
  if (!vram) vram=(uint8_t*)ps_malloc(0x10000);
  if (!vram) vram=(uint8_t*)malloc(0x10000);
  active=this;
  m6803_ext_read_fn=audio_read_cb;
  m6803_ext_write_fn=audio_write_cb;
  m6803_port_read_fn=nullptr;
  m6803_port_write_fn=nullptr;
  reset();
}

void qix::reset() {
  machineBase::reset();
  shared=memory; data_ram=memory+0x400; nvram=memory+0x800;
  // Complete initialized CMOS image.  Merely setting the documented 55/AA
  // signature is insufficient: Qix also validates operator tables.
  memcpy(nvram,qix_nvram_default,0x400);
  if (vram) memset(vram,0,0x10000);
  memset(pia,0,sizeof(pia)); memset(&audio_pia,0,sizeof(audio_pia));
  data_to_sound=sound_to_data=0xff; dac_value=0x80; volume_value=0;
  __atomic_store_n(&dac_read_pos,0,__ATOMIC_RELAXED);
  __atomic_store_n(&dac_write_pos,0,__ATOMIC_RELAXED);
  dac_sample_phase=dac_overruns=dac_underruns=0;
  dac_last_sample=0; dac_playing=false; debug_audio_time=micros(); debug_cpu_us=0;
  dac_resample_phase=dac_resample_count=0; dac_resample_step=65536;
  debug_frames=vram_writes=sound_commands=sound_replies=0;
  debug_last_data_pc=debug_fail_from=debug_preirq_pc=0; debug_test_restarts=0;
  debug_video_services=debug_cc47_entries=0;
  memset(debug_video_trace,0,sizeof(debug_video_trace)); debug_video_trace_pos=0;
  vram_addr=0; palette_bank=0;
  crtc_addr=0; memset(crtc_regs,0,sizeof(crtc_regs));
  for(int i=0;i<256;i++) palette[i]=pen(i);
  m6809_reset(&data_cpu); m6809_reset(&video_cpu);
  // Qix ROMs are not encrypted. Use the core's existing fast fetch window
  // now that the video-bank fault is isolated; keep MMIO on the callbacks.
  data_cpu.rom_direct=qix_main_rom;
  data_cpu.rom_base=0xc000; data_cpu.rom_size=sizeof(qix_main_rom);
  video_cpu.rom_direct=qix_video_rom;
  video_cpu.rom_base=0xc800; video_cpu.rom_size=sizeof(qix_video_rom);
  audio_cpu.is_6802=1;
  m6803_reset(&audio_cpu);
}

uint8_t qix::pia_read(pia_t &p,uint8_t r,uint8_t ina,uint8_t inb) {
  switch(r&3) {
    case 0: { uint8_t v=(p.cra&4)?((p.outa&p.ddra)|(ina&~p.ddra)):p.ddra; p.cra&=0x3f; return v; }
    case 1: return p.cra;
    case 2: { uint8_t v=(p.crb&4)?((p.outb&p.ddrb)|(inb&~p.ddrb)):p.ddrb; p.crb&=0x3f; return v; }
    default:return p.crb;
  }
}
void qix::pia_write(pia_t &p,uint8_t r,uint8_t v) {
  switch(r&3) {
    case 0: if(p.cra&4)p.outa=v;else p.ddra=v; break;
    case 1: p.cra=v; break;
    case 2: if(p.crb&4)p.outb=v;else p.ddrb=v; break;
    case 3: p.crb=v; break;
  }
}

uint8_t IRAM_ATTR qix::data_read(uint16_t a) {
  if(a>=0x8000&&a<0x8400)return shared[a&0x3ff];
  if(a>=0x8400&&a<0x8800)return data_ram[a&0x3ff];
  if(a>=0xc000)return qix_main_rom[a-0xc000];
  // 6850 ACIA is only fitted for factory diagnostics.  MAME maps this
  // range as nopr(); returning 0xff sets the status sign bit and makes the
  // Qix power-on test branch back to C006 forever (observed at PC FD51).
  if(a>=0x8800&&a<0x8c00)return 0x00;
  if(a>=0x8c00&&a<0x9000){ if(a&1)data_cpu.firq_pending=0; else m6809_firq(&video_cpu); return 0xff; }
  uint8_t k=input->buttons_get(), p1=0xff, coin=0xff;
  if(k&BUTTON_UP)p1&=~1; if(k&BUTTON_RIGHT)p1&=~2; if(k&BUTTON_DOWN)p1&=~4; if(k&BUTTON_LEFT)p1&=~8;
  if(k&BUTTON_FIRE)p1&=~0x80; if(k&BUTTON_START)p1&=~0x40; if(k&BUTTON_COIN)coin&=~0x10;
  if(a>=0x9000&&a<0x9400)return pia_read(pia[3],a&3,sound_to_data,0xff);
  if(a>=0x9400&&a<0x9800)return pia_read(pia[0],a&3,p1,coin);
  if(a>=0x9800&&a<0x9c00)return pia_read(pia[1],a&3,0xff,0xff);
  if(a>=0x9c00&&a<0xa000)return pia_read(pia[2],a&3,0xff,0xff);
  return 0xff;
}
void IRAM_ATTR qix::data_write(uint16_t a,uint8_t v) {
  if(a>=0x8000&&a<0x8400){shared[a&0x3ff]=v;return;}
  if(a>=0x8400&&a<0x8800){data_ram[a&0x3ff]=v;return;}
  if(a>=0x8c00&&a<0x9000){if(a&1)data_cpu.firq_pending=0;else m6809_firq(&video_cpu);return;}
  if(a>=0x9000&&a<0x9400){
    uint8_t reg=a&3;
    const uint8_t old_cra=pia[3].cra;
    bool port_a=(reg==0)&&(pia[3].cra&4);
    bool port_b=(reg==2)&&(pia[3].crb&4);
    pia_write(pia[3],reg,v);
    if(port_a){
      data_to_sound=v;
      sound_commands++;
    }
    // Port A also carries live waveform parameters: writing it must NOT
    // restart the sound program. Only CA2 -> CA1's selected edge does so.
    // Qix explicitly pulses manual CA2 low/high (CRA 0x34, then 0x3c).
    if(reg==1 && (v&0x30)==0x30 && ((old_cra^v)&8) &&
       bool(v&8)==bool(audio_pia.cra&2)){
      audio_pia.cra|=0x80;
      if(audio_pia.cra&1)m6803_irq(&audio_cpu);
    }
    if(port_b) volume_value=v;
    return;
  }
  if(a>=0x9400&&a<0x9800){pia_write(pia[0],a&3,v);return;}
  if(a>=0x9800&&a<0x9c00){pia_write(pia[1],a&3,v);return;}
  if(a>=0x9c00&&a<0xa000)pia_write(pia[2],a&3,v);
}

uint8_t IRAM_ATTR qix::video_read(uint16_t a) {
  // A15 comes from bit 7 of the HIGH address latch at 0x9402.
  if(a<0x8000)return vram[a+(vram_addr&0x8000)];
  if(a<0x8400)return shared[a&0x3ff];
  if(a<0x8800)return nvram[a&0x3ff];
  if(a>=0x8c00&&a<0x9000){if(a&1)video_cpu.firq_pending=0;else m6809_firq(&data_cpu);return 0xff;}
  if(a>=0x9000&&a<0x9400)return 0xff;
  if(a==0x9400)return vram[vram_addr];
  if(a==0x9402)return vram_addr>>8;
  if(a==0x9403)return vram_addr;
  if(a>=0x9800&&a<0x9c00){
    // The 6845 character clock is half the 6809E clock.  Qix programs a
    // 41-character line and 33 rows of 8 rasters (264 physical lines).
    // Derive the latch from the live CRTC registers; the video ROM polls
    // this value and will stop servicing the shared command queue if its
    // timing is unrelated to the programmed display.
    uint16_t line_cycles=(uint16_t)(crtc_regs[0]+1)*2;
    uint16_t frame_lines=(uint16_t)(crtc_regs[4]+1)*(crtc_regs[9]+1)+crtc_regs[5];
    if(line_cycles<64)line_cycles=82;
    if(frame_lines<240)frame_lines=264;
    return (video_cpu.total_cycles/line_cycles)%frame_lines;
  }
  if(a>=0x9c00&&a<0xa000)return (a&1)?crtc_regs[crtc_addr&0x1f]:crtc_addr;
  if(a>=0xc800)return qix_video_rom[a-0xc800];
  return 0xff;
}
void IRAM_ATTR qix::video_write(uint16_t a,uint8_t v) {
  if(a<0x8000){vram[a+(vram_addr&0x8000)]=v;vram_writes++;game_started=1;return;}
  if(a<0x8400){shared[a&0x3ff]=v;return;}
  if(a<0x8800){nvram[a&0x3ff]=v;return;}
  if(a>=0x8800&&a<0x8c00){palette_bank=v&3;return;}
  if(a>=0x8c00&&a<0x9000){if(a&1)video_cpu.firq_pending=0;else m6809_firq(&data_cpu);return;}
  if(a>=0x9000&&a<0x9400){palette[a&0x3ff]=pen(v);return;}
  if(a==0x9400){vram[vram_addr]=v;vram_writes++;game_started=1;return;}
  if(a==0x9402){vram_addr=(vram_addr&0xff)|((uint16_t)v<<8);return;}
  if(a==0x9403)vram_addr=(vram_addr&0xff00)|v;
  if(a>=0x9c00&&a<0xa000){
    if(a&1)crtc_regs[crtc_addr&0x1f]=v;
    else crtc_addr=v&0x1f;
  }
}

unsigned char IRAM_ATTR qix::m6809_read(m6809_state *c,uint16_t a){return c==&data_cpu?data_read(a):video_read(a);}
void IRAM_ATTR qix::m6809_write(m6809_state *c,uint16_t a,uint8_t v){if(c==&data_cpu)data_write(a,v);else video_write(a,v);}
unsigned char qix::m6809_read_opcode(m6809_state *c,uint16_t a){
  if(c==&data_cpu){
    if(a==0xc006){debug_fail_from=debug_last_data_pc;debug_test_restarts++;}
    debug_last_data_pc=a;
  }else{
    debug_video_trace[debug_video_trace_pos++%12]=a;
    if(a==0xc8ad)debug_video_services++;
    if(a==0xcc47)debug_cc47_entries++;
  }
  return m6809_read(c,a);
}

uint8_t IRAM_ATTR qix::audio_read_cb(uint16_t a){return active?active->audio_read(a):0xff;}
void IRAM_ATTR qix::audio_write_cb(uint16_t a,uint8_t v){if(active)active->audio_write(a,v);}

uint8_t IRAM_ATTR qix::audio_read(uint16_t a) {
  if(a>=0xf800)return qix_sound_rom[a-0xf800];
  if(a>=0x4000&&a<0x8000)return pia_read(audio_pia,a&3,data_to_sound,0xff);
  return 0xff;
}

void IRAM_ATTR qix::audio_write(uint16_t a,uint8_t v) {
  if(a>=0x4000&&a<0x8000){
    uint8_t reg=a&3;
    const uint8_t old_cra=audio_pia.cra;
    bool port_a=(reg==0)&&(audio_pia.cra&4);
    bool port_b=(reg==2)&&(audio_pia.crb&4);
    pia_write(audio_pia,reg,v);
    if(port_a){
      sound_to_data=v;
      sound_replies++;
    }
    if(reg==1 && (v&0x30)==0x30 && ((old_cra^v)&8) &&
       bool(v&8)==bool(pia[3].cra&2)){
      pia[3].cra|=0x80;
      if(pia[3].cra&1)m6809_irq(&data_cpu);
    }
    if(port_b)dac_value=v;
  }
}

// Keep the per-instruction dispatch/memory path out of the flash cache:
// it must produce a complete 24 kHz audio stream within each frame.
void IRAM_ATTR qix::run_frame() {
  const uint32_t cpu_start=micros();
  // Both 6809E CPUs run at 1.25 MHz and Qix synchronizes them through a
  // two-byte producer/consumer counter in shared RAM.  Execute one
  // instruction at a time and budget real CPU cycles: four-instruction
  // batches let one CPU pass the other's handshake window and also made
  // the 6809 core's accumulated cycle count unsuitable for the scanline
  // latch.
  int data_cycles=0,video_cycles=0,audio_cycles=0;
  while(data_cycles<20834 || video_cycles<20834 || audio_cycles<15360){
    if(data_cycles<20834)data_cycles+=m6809_step(&data_cpu,1);
    if(video_cycles<20834)video_cycles+=m6809_step(&video_cpu,1);
    if(audio_cycles<15360){
      const int used=m6803_step(&audio_cpu);
      audio_cycles+=used;
      // Existing 15360 cycles/frame at 60 Hz = 921600 audio CPU Hz.
      // Sample in emulated CPU time, preserving the waveform within a frame.
      dac_sample_phase+=(uint32_t)used*24000U;
      while(dac_sample_phase>=921600U){
        dac_sample_phase-=921600U;
        queue_dac_sample();
      }
    }
  }
  debug_preirq_pc=data_cpu.PC;
  // CRTC VSYNC drives CB1 of the sound/data PIA.  Expose the PIA IRQ flag
  // as well as raising the CPU line: the ROM checks CRB bit 7 in its ISR.
  pia[3].crb|=0x80;
  m6809_irq(&data_cpu);

  debug_cpu_us+=micros()-cpu_start;
  if(++debug_frames>=60){
    debug_frames=0;
    const uint32_t now=micros(), elapsed=now-debug_audio_time;
    debug_audio_time=now;
    const uint32_t fps100=elapsed?6000000000ULL/elapsed:0;
    const uint32_t rd=__atomic_load_n(&dac_read_pos,__ATOMIC_ACQUIRE);
    const uint32_t wr=__atomic_load_n(&dac_write_pos,__ATOMIC_ACQUIRE);
    Serial.printf("QIX PCM fps=%lu.%02lu cpu_us=%lu queued=%lu under=%lu over=%lu\n",
      (unsigned long)(fps100/100),(unsigned long)(fps100%100),
      (unsigned long)(debug_cpu_us/60),
      (unsigned long)((wr-rd)&(DAC_BUFFER_SIZE-1)),
      (unsigned long)__atomic_load_n(&dac_underruns,__ATOMIC_RELAXED),
      (unsigned long)dac_overruns);
    debug_cpu_us=0;
    uint8_t keys=input->buttons_get();
    uint16_t irq_return=0xffff;
    if((data_cpu.CC&M6809_CC_E) && data_cpu.S>=0x8400 && data_cpu.S<=0x87f4)
      irq_return=((uint16_t)data_ram[data_cpu.S+10-0x8400]<<8)|data_ram[data_cpu.S+11-0x8400];
    uint8_t yp5=(video_cpu.Y>=0x8000&&video_cpu.Y<0x8400)?shared[(video_cpu.Y+5)&0x3ff]:0xff;
    Serial.printf("QIX data=%04X pre=%04X ret=%04X S=%04X cc=%02X h=%u irq=%u firq=%u | video=%04X A=%02X B=%02X X=%04X Y=%04X y5=%02X dp=%02X cc=%02X h=%u irq=%u firq=%u | vram=%lu addr=%04X pix=%02X sync=%02X/%02X svc=%lu cc47=%lu | audio=%04X snd=%lu/%lu keys=%02X\n",
      data_cpu.PC,debug_preirq_pc,irq_return,data_cpu.S,data_cpu.CC,data_cpu.halted,data_cpu.irq_pending,data_cpu.firq_pending,
      video_cpu.PC,video_cpu.A,video_cpu.B,video_cpu.X,video_cpu.Y,yp5,video_cpu.DP,video_cpu.CC,
      video_cpu.halted,video_cpu.irq_pending,video_cpu.firq_pending,
      (unsigned long)vram_writes,vram_addr,vram[vram_addr],shared[0x31a],shared[0x31b],
      (unsigned long)debug_video_services,(unsigned long)debug_cc47_entries,
      audio_cpu.PC,(unsigned long)sound_commands,(unsigned long)sound_replies,keys);
    // Per-opcode tracing is bypassed by the direct ROM windows.
  }
}

void qix::render_row(short row) {
  if(!vram||row<2||row>33)return;
  // Read eight adjacent VRAM bytes together: traversing PSRAM at a
  // 256-byte stride separately for each line needlessly churns the cache.
  const int native_x=(row-2)*8;
  const uint16_t *pens=palette+(palette_bank<<8);
  // Preserve the rotation and symmetric eight-pixel crop.
  for(int x=0;x<240;x++){
    const uint8_t *src=vram+(((247-x)<<8)|native_x);
    for(int y=0;y<8;y++)frame_buffer[y*240+x]=pens[src[y]];
  }
}

const hiscore_region_S *qix::hiscoreRegions(unsigned char *count) {
  // Match the lowered 10000-point default before restoring saved records.
  static const hiscore_region_S r[]={{0x86c4,0x3c,0x01,0x58}};
  *count=1; return r;
}
unsigned char qix::hiscoreRead(unsigned short a){return nvram[a-0x8400];}
void qix::hiscoreWrite(unsigned short a,unsigned char v){nvram[a-0x8400]=v;}

void qix::queue_dac_sample() {
  const uint32_t wr=__atomic_load_n(&dac_write_pos,__ATOMIC_RELAXED);
  const uint32_t next=(wr+1)&(DAC_BUFFER_SIZE-1);
  if(next==__atomic_load_n(&dac_read_pos,__ATOMIC_ACQUIRE)){
    ++dac_overruns; return;
  }
  int sample=((int)dac_value-128)*4;
  // Qix uses two four-bit passive attenuators.  Zero means full volume;
  // use the louder side for the mono Galagino output.
  uint8_t l=volume_value>>4, r=volume_value&15, att=l<r?l:r;
  dac_samples[wr]=sample*(16-att)/16;
  __atomic_store_n(&dac_write_pos,next,__ATOMIC_RELEASE);
}

int qix::renderDacSample() {
  const uint32_t rd=__atomic_load_n(&dac_read_pos,__ATOMIC_RELAXED);
  const uint32_t wr=__atomic_load_n(&dac_write_pos,__ATOMIC_ACQUIRE);
  const uint32_t queued=(wr-rd)&(DAC_BUFFER_SIZE-1);
  // Two frames absorb scheduling bursts. The LCD/emulation and I2S clocks
  // are independent; gently track their small rate difference rather than
  // periodically dropping samples or repeating an empty buffer's last value.
  if(!dac_playing){
    if(queued<800)return 0;
    dac_playing=true;
  }
  if(queued<2){
    __atomic_fetch_add(&dac_underruns,1,__ATOMIC_RELAXED);
    return dac_last_sample;
  }
  if((dac_resample_count++&63)==0){
    int32_t correction=((int32_t)queued-800)*8;
    if(correction>6553)correction=6553;
    if(correction< -6553)correction= -6553;
    dac_resample_step+=(65536+correction-dac_resample_step)/16;
  }
  const int32_t first=dac_samples[rd];
  const int32_t second=dac_samples[(rd+1)&(DAC_BUFFER_SIZE-1)];
  dac_last_sample=first+(second-first)*(int32_t)dac_resample_phase/65536;
  dac_resample_phase+=dac_resample_step;
  const uint32_t advance=dac_resample_phase>>16;
  dac_resample_phase&=65535;
  __atomic_store_n(&dac_read_pos,(rd+advance)&(DAC_BUFFER_SIZE-1),__ATOMIC_RELEASE);
  return dac_last_sample;
}
