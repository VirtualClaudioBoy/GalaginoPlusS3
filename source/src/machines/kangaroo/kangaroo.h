#ifndef KANGAROO_H
#define KANGAROO_H

#include "../machineBase.h"
#include "kangaroo_mcu.h"

#ifndef KANGAROO_PROFILE
#define KANGAROO_PROFILE 1
#endif

class kangaroo : public machineBase {
public:
  ~kangaroo() { free(vram); free(frames); }
  signed char machineType() override { return MCH_KANGAROO; }
  const char *hiscoreKey() override { return "kangaroo"; }
  const hiscore_region_S *hiscoreRegions(unsigned char *count) override;
  // The score manager runs on the display core while either Z80 may be active.
  unsigned char hiscoreRead(unsigned short addr) override { return memory[addr - 0xe000]; }
  void hiscoreWrite(unsigned short addr, unsigned char value) override { memory[addr - 0xe000] = value; }
  const unsigned short *logo() override;
  void start() override;
  void reset() override;
  unsigned char IRAM_ATTR rdZ80(unsigned short addr) override;
  unsigned char IRAM_ATTR opZ80(unsigned short addr) override;
  void IRAM_ATTR wrZ80(unsigned short addr, unsigned char value) override;
  unsigned char inZ80(unsigned short port) override { return rdZ80(port); }
  void outZ80(unsigned short port, unsigned char value) override { wrZ80(port, value); }
  void run_frame() override;
  void prepare_frame() override;
  void render_row(short row) override;

private:
  void video_write(uint16_t offset, uint8_t data, uint8_t mask);
  void blit();
  void IRAM_ATTR run_cpu(unsigned index, int cycles);
  uint32_t *vram = nullptr;
  struct video_frame { uint32_t pixels[0x4000]; uint8_t control[11]; };
  video_frame *frames = nullptr;
  uint32_t middle_buffer = 1;
  uint8_t draw_buffer = 2, display_buffer = 0;
  uint16_t pens[256] = {};
  uint8_t control[11] = {}, sound_latch = 0, ay_address = 0;
  uint8_t keys = 0;
  kangaroo_mcu mcu;
  int cycle_debt[2] = {};
#if KANGAROO_PROFILE
  uint32_t profile_start = 0, profile_frames = 0;
  uint32_t profile_main = 0, profile_sound = 0, profile_copy = 0, profile_blit = 0;
  uint32_t profile_video = 0;
#endif
};

#endif
