#ifndef QIX_H
#define QIX_H

#include "../machineBase.h"
#include "../../cpus/m6803/m6803.h"
#include "qix_rom.h"
#include "qix_nvram.h"
#include "qix_logo.h"

class qix : public machineBase {
public:
  signed char machineType() override { return MCH_QIX; }
  signed char videoFlipY() override { return 1; }
  void start() override;
  void reset() override;
  void run_frame() override;
  void render_row(short row) override;
  const unsigned short *logo() override { return qix_logo; }
  unsigned char m6809_read(m6809_state *cpu, uint16_t addr) override;
  void m6809_write(m6809_state *cpu, uint16_t addr, uint8_t val) override;
  unsigned char m6809_read_opcode(m6809_state *cpu, uint16_t addr) override;
  const char *hiscoreKey() override { return "qix"; }
  const hiscore_region_S *hiscoreRegions(unsigned char *count) override;
  unsigned char hiscoreRead(unsigned short addr) override;
  void hiscoreWrite(unsigned short addr, unsigned char value) override;
  int renderDacSample() override;

private:
  struct pia_t { uint8_t outa, outb, ddra, ddrb, cra, crb; };
  uint8_t pia_read(pia_t &p, uint8_t reg, uint8_t ina, uint8_t inb);
  void pia_write(pia_t &p, uint8_t reg, uint8_t value);
  uint8_t data_read(uint16_t addr);
  void data_write(uint16_t addr, uint8_t value);
  uint8_t video_read(uint16_t addr);
  void video_write(uint16_t addr, uint8_t value);
  static uint8_t audio_read_cb(uint16_t addr);
  static void audio_write_cb(uint16_t addr, uint8_t value);
  uint8_t audio_read(uint16_t addr);
  void audio_write(uint16_t addr, uint8_t value);
  uint16_t pen(uint8_t raw) const;
  void queue_dac_sample();

  m6809_state data_cpu{}, video_cpu{};
  m6803_state audio_cpu{};
  uint8_t *vram = nullptr;
  uint8_t *shared = nullptr, *data_ram = nullptr, *nvram = nullptr;
  uint16_t palette[1024]{};
  pia_t pia[4]{};
  pia_t audio_pia{};
  uint8_t data_to_sound=0xff, sound_to_data=0xff;
  volatile uint8_t dac_value=0x80, volume_value=0;
  // One producer (emulation) and one consumer (I2S), published atomically.
  static constexpr uint32_t DAC_BUFFER_SIZE = 2048;
  int16_t dac_samples[DAC_BUFFER_SIZE]{};
  uint32_t dac_read_pos=0, dac_write_pos=0;
  uint32_t dac_sample_phase=0, dac_overruns=0, dac_underruns=0;
  int16_t dac_last_sample=0;
  uint32_t dac_resample_phase=0, dac_resample_count=0;
  int32_t dac_resample_step=65536;
  bool dac_playing=false;
  uint32_t debug_audio_time=0, debug_cpu_us=0;
  uint32_t debug_frames=0, vram_writes=0, sound_commands=0, sound_replies=0;
  uint16_t debug_last_data_pc=0, debug_fail_from=0;
  uint16_t debug_preirq_pc=0;
  uint32_t debug_test_restarts=0;
  uint32_t debug_video_services=0, debug_cc47_entries=0;
  uint16_t debug_video_trace[12]{};
  uint8_t debug_video_trace_pos=0;
  static qix *active;
  uint16_t vram_addr = 0;
  uint8_t palette_bank = 0;
  uint8_t crtc_addr=0, crtc_regs[32]{};
};

#endif
