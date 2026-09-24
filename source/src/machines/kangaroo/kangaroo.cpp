// Video and memory map adapted from MAME kangaroo.cpp (BSD-3-Clause).
// Copyright holders: Ville Laitinen, Aaron Giles. See LICENSE.MAME.
#include "kangaroo.h"
#include "kangaroo_rom.h"
#include "kangaroo_logo.h"
#include "../../cpus/z80/Tables.h"
#include <esp_heap_caps.h>

extern "C" void StepZ80(Z80 *);

static_assert(sizeof(kangaroo_main_rom) == 0x6000, "Kangaroo main ROM size");
static_assert(sizeof(kangaroo_sound_rom) == 0x1000, "Kangaroo sound ROM size");
static_assert(sizeof(kangaroo_gfx_rom) == 0x4000, "Kangaroo graphics ROM size");

// Internal DRAM: opcode/data reads no longer contend with PSRAM video traffic.
static uint8_t main_program[0x6000], sound_program[0x1000];

const unsigned short *kangaroo::logo() { return kangaroo_logo; }

const hiscore_region_S *kangaroo::hiscoreRegions(unsigned char *count) {
  // MAME plugins/hiscore/hiscore.dat, Kangaroo parent set. The overlapping
  // third region is an initialization guard: default top score is 005000.
  static const hiscore_region_S regions[] = {
    {0xe1a0, 0x3c, 0x00, 0x00},
    {0xe300, 0x06, 0x00, 0x00},
    {0xe302, 0x01, 0x05, 0x05}
  };
  *count = sizeof(regions) / sizeof(regions[0]);
  return regions;
}

void kangaroo::start() {
  if (!vram) {
    vram = (uint32_t*)heap_caps_malloc(0x10000, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (vram) printf("Kangaroo: 64 KiB VRAM in internal RAM\n");
    else vram = (uint32_t*)ps_malloc(0x10000);
  }
  if (!frames) frames = (video_frame*)ps_malloc(3 * sizeof(video_frame));
  if (!vram || !frames) {
    printf("Kangaroo: no video memory\n");
    return;
  }
  reset();
}

void kangaroo::reset() {
  memset(cpu, 0, sizeof(cpu));
  machineBase::reset();
  memcpy(main_program, kangaroo_main_rom, sizeof(main_program));
  memcpy(sound_program, kangaroo_sound_rom, sizeof(sound_program));
  if (vram) memset(vram, 0, 0x10000);
  if (frames) memset(frames, 0, 3 * sizeof(video_frame));
  middle_buffer = 1; draw_buffer = 2; display_buffer = 0;
  memset(control, 0, sizeof(control));
  memset(cycle_debt, 0, sizeof(cycle_debt));
  sound_latch = ay_address = keys = 0;
  soundregs[7] = 0x3f;
  mcu.reset();
  current_cpu = 0;
  game_started = 1;
#if KANGAROO_PROFILE
  profile_start = micros(); profile_frames = 0;
  profile_main = profile_sound = profile_copy = profile_blit = profile_video = 0;
#endif
}

unsigned char IRAM_ATTR kangaroo::opZ80(unsigned short addr) {
  if (current_cpu == 0 && addr < 0x6000) return main_program[addr];
  if (current_cpu == 1 && addr < 0x1000) return sound_program[addr];
  return rdZ80(addr);
}

unsigned char IRAM_ATTR kangaroo::rdZ80(unsigned short addr) {
  if (current_cpu == 1) {
    if (addr < 0x1000) return sound_program[addr];
    if ((addr & 0xf000) == 0x4000) return memory[0x400 + (addr & 0x3ff)];
    if ((addr & 0xf000) == 0x6000) return sound_latch;
    return 0xff;
  }
  if (addr < 0x6000) return main_program[addr];
  if (addr >= 0xc000 && addr < 0xe000)
    return kangaroo_gfx_rom[((control[8] & 5) ? 0 : 0x2000) + (addr & 0x1fff)];
  if (addr >= 0xe000 && addr < 0xe400) return memory[addr & 0x3ff];
  if (addr >= 0xe400 && addr < 0xe800) return 0; // 3 lives, easy, 1 coin/credit
  switch (addr & 0xff00) {
    case 0xec00: return ((keys & BUTTON_COIN) ? 8 : 0) | ((keys & BUTTON_START) ? 2 : 0);
    case 0xed00: return ((keys & BUTTON_RIGHT) ? 1 : 0) | ((keys & BUTTON_LEFT) ? 2 : 0) |
                       ((keys & BUTTON_UP) ? 4 : 0) | ((keys & BUTTON_DOWN) ? 8 : 0) |
                       ((keys & BUTTON_FIRE) ? 16 : 0);
    case 0xee00: return 0; // upright, second player controls unused
    case 0xef00: return mcu.ports[0];
  }
  return 0xff;
}

void IRAM_ATTR kangaroo::wrZ80(unsigned short addr, unsigned char value) {
  if (current_cpu == 1) {
    switch (addr & 0xf000) {
      case 0x4000: memory[0x400 + (addr & 0x3ff)] = value; break;
      case 0x7000: soundregs[ay_address] = value; break;
      case 0x8000: ay_address = value & 15; break;
    }
    return;
  }
  if (addr >= 0x8000 && addr < 0xc000) { video_write(addr & 0x3fff, value, control[8]); return; }
  if (addr >= 0xe000 && addr < 0xe400) { memory[addr & 0x3ff] = value; return; }
  if ((addr & 0xfc00) == 0xe800 && (addr & 15) <= 10) {
    control[addr & 15] = value;
    if ((addr & 15) == 5) blit();
  }
  if ((addr & 0xff00) == 0xec00) sound_latch = value;
  if ((addr & 0xff00) == 0xef00) mcu.data = value & 15;
}

void kangaroo::video_write(uint16_t offset, uint8_t data, uint8_t mask) {
  if (!vram) return;
  // Four pixels per word, A in the low nibble and B in the high nibble.
  uint32_t expanded = 0, layers = 0;
  for (unsigned p = 0; p < 4; p++)
    expanded |= uint32_t((((data >> p) & 1) * 0x55) | (((data >> (p + 4)) & 1) * 0xaa)) << (p * 8);
  if (mask & 8) layers |= 0x30303030;
  if (mask & 4) layers |= 0xc0c0c0c0;
  if (mask & 2) layers |= 0x03030303;
  if (mask & 1) layers |= 0x0c0c0c0c;
  vram[offset] = (vram[offset] & ~layers) | (expanded & layers);
}

void kangaroo::blit() {
#if KANGAROO_PROFILE
  const uint32_t begin = micros();
#endif
  uint16_t src = control[0] | (control[1] << 8);
  uint16_t dst = control[2] | (control[3] << 8);
  uint8_t mask = control[8];
  if (mask & 0x0c) mask |= 0x0c;
  if (mask & 0x03) mask |= 0x03;
  for (unsigned y = 0; y <= control[5]; y++, dst += 256) {
    for (unsigned x = 0; x <= control[4]; x++, src++) {
      video_write((dst + x) & 0x3fff, kangaroo_gfx_rom[src & 0x1fff], mask & 5);
      video_write((dst + x) & 0x3fff, kangaroo_gfx_rom[0x2000 + (src & 0x1fff)], mask & 10);
    }
  }
#if KANGAROO_PROFILE
  profile_blit += micros() - begin;
#endif
}

void IRAM_ATTR kangaroo::run_cpu(unsigned index, int cycles) {
  current_cpu = index;
  cycle_debt[index] += cycles;
  while (cycle_debt[index] > 0) {
    if (cpu[index].IFF & IFF_HALT) {
      if (index == 0) {
        mcu.run(4);
        if (mcu.nmi_pending) {
          mcu.nmi_pending = false; IntZ80(&cpu[0], INT_NMI); cycle_debt[0] -= 11; mcu.run(11);
        }
      }
      cycle_debt[index] -= 4;
      continue;
    }
    uint8_t opcode = opZ80(cpu[index].PC.W);
    // StepZ80 accounts for prefixes and taken branches, but omits base timing.
    cpu[index].ICount = 0;
    StepZ80(&cpu[index]);
    int extra = cpu[index].ICount < 0 ? -cpu[index].ICount : 0;
    const int elapsed = Cycles[opcode] + extra;
    cycle_debt[index] -= elapsed;
    if (index == 0) {
      mcu.run(elapsed);
      if (mcu.nmi_pending) {
        mcu.nmi_pending = false;
        IntZ80(&cpu[0], INT_NMI);
        cycle_debt[0] -= 11;
        mcu.run(11);
      }
    }
  }
}

void kangaroo::run_frame() {
  if (!vram || !frames) return;
  keys = input->buttons_get();
  // 2.5 MHz / (10 MHz / 640 / 260) = 41600 cycles per native frame.
  // Interleave both CPUs once per scanline so sound commands are not lost.
  for (unsigned line = 0; line < 260; line++) {
    if (line == 248) {
#if KANGAROO_PROFILE
      const uint32_t begin = micros();
#endif
      // Capture before the IRQ handler erases/redraws objects for the next
      // frame. Triple buffering keeps the display task off live blitter RAM.
      memcpy(frames[draw_buffer].pixels, vram, 0x10000);
      memcpy(frames[draw_buffer].control, control, sizeof(control));
      draw_buffer = __atomic_exchange_n(&middle_buffer, draw_buffer | 4, __ATOMIC_ACQ_REL) & 3;
#if KANGAROO_PROFILE
      profile_copy += micros() - begin;
#endif
      for (unsigned c = 0; c < 2; c++) {
        current_cpu = c;
        if (cpu[c].IFF & IFF_1) {
          IntZ80(&cpu[c], INT_IRQ); cycle_debt[c] -= 13;
          if (c == 0) mcu.run(13);
        }
      }
    }
#if KANGAROO_PROFILE
    uint32_t begin = micros();
#endif
    run_cpu(0, 160);
#if KANGAROO_PROFILE
    profile_main += micros() - begin;
    begin = micros();
#endif
    run_cpu(1, 160);
#if KANGAROO_PROFILE
    profile_sound += micros() - begin;
#endif
  }
  current_cpu = 0;
#if KANGAROO_PROFILE
  if (++profile_frames == 60) {
    const uint32_t now = micros(), elapsed = now - profile_start;
    const uint32_t video = __atomic_exchange_n(&profile_video, 0, __ATOMIC_RELAXED);
    printf("Kangaroo: %.2f FPS | Z80+MCU %.2f ms | blit %.2f | copy %.2f | soundCPU %.2f | video %.2f | keys %02x\n",
           60000000.0 / elapsed, (profile_main - profile_blit) / 60000.0,
           profile_blit / 60000.0, profile_copy / 60000.0, profile_sound / 60000.0,
           video / 60000.0, keys);
    profile_start = now; profile_frames = 0;
    profile_main = profile_sound = profile_copy = profile_blit = 0;
  }
#endif
}

void kangaroo::prepare_frame() {
  if (__atomic_load_n(&middle_buffer, __ATOMIC_ACQUIRE) & 4)
    display_buffer = __atomic_exchange_n(&middle_buffer, display_buffer, __ATOMIC_ACQ_REL) & 3;
  if (!frames) return;
  const uint8_t *control = frames[display_buffer].control;
  const bool ena = control[9] & 8, enb = control[9] & 4;
  const bool pria = !(control[9] & 2), prib = !(control[9] & 1);
  const uint8_t ma = (control[10] & 0x28) >> 3, mb = control[10] & 7;
  // All 16x16 plane combinations share the same priority and shading rules.
  for (unsigned i = 0; i < 256; i++) {
    uint8_t a = i >> 4, b = i & 15;
    uint8_t full = 0, shaded = 0;
    if (ena && (pria || b == 0)) full |= a;
    if (enb && (prib || a == 0)) full |= b;
    if (ena && (pria || b == 0)) { if (!(a & 8)) a &= ma; shaded |= a; }
    if (enb && (prib || a == 0)) { if (!(b & 8)) b &= mb; shaded |= b; }
    // MAME BGR_3BIT: bit 2 = red, bit 1 = green, bit 0 = blue.
    unsigned r = ((full & 4) != 0) + ((shaded & 4) != 0);
    unsigned g = ((full & 2) != 0) + ((shaded & 2) != 0);
    unsigned bl = ((full & 1) != 0) + ((shaded & 1) != 0);
    uint16_t color = ((r * 31 / 2) << 11) | ((g * 63 / 2) << 5) | (bl * 31 / 2);
    pens[i] = (color >> 8) | (color << 8);
  }
}

void kangaroo::render_row(short row) {
  if (!frames || row < 2 || row > 33) return;
#if KANGAROO_PROFILE
  const uint32_t begin = micros();
#endif
  const uint8_t *control = frames[display_buffer].control;
  const uint32_t *vram = frames[display_buffer].pixels;
  // ROT90, 240x256 visible pixels, centered vertically in the 240x288 viewport.
  // The board emits two subpixels per logical pixel; average their RGB levels
  // to preserve the color shading when reducing 512 native columns to 256.
  const uint8_t xa = (control[9] & 0x20) ? 255 : 0;
  const uint8_t xb = (control[9] & 0x10) ? 255 : 0;
  for (int y = 0; y < 8; y++) {
    const uint8_t native_x = (row - 2) * 8 + y;
    const uint8_t ax = control[7] + (native_x ^ xa), bx = native_x ^ xb;
    for (int x = 0; x < 240; x++) {
      const uint8_t native_y = 247 - x;
      const uint8_t ay = control[6] + (native_y ^ xa), by = native_y ^ xb;
      uint8_t a = (vram[ay + 256 * (ax / 4)] >> (8 * (ax % 4))) & 15;
      uint8_t b = (vram[by + 256 * (bx / 4)] >> (8 * (bx % 4) + 4)) & 15;
      frame_buffer[y * 240 + x] = pens[(a << 4) | b];
    }
  }
#if KANGAROO_PROFILE
  __atomic_fetch_add(&profile_video, micros() - begin, __ATOMIC_RELAXED);
#endif
}
