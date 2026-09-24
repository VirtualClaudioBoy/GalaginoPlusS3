#ifndef KANGAROO_MCU_H
#define KANGAROO_MCU_H

#include <stdint.h>
#include <esp_attr.h>

// MB8841 with Kangaroo's board wiring. Executes the original internal ROM.
// External interrupt, external timer clock and serial input are unconnected.
// The internal serial timer remains available, with a zero input level.
class kangaroo_mcu {
public:
  void reset();
  void IRAM_ATTR run(int clocks);
  uint8_t data = 0, ports[4] = {};
  bool nmi_pending = false;

private:
  void IRAM_ATTR execute_run();
  void IRAM_ATTR burn_cycles(int cycles);
  void IRAM_ATTR increment_timer();
  void IRAM_ATTR write_pla(uint8_t index);
  void IRAM_ATTR write_r(unsigned port, uint8_t value);
  uint8_t IRAM_ATTR read_k();
  void pio_enable(uint8_t value) { m_pio = value; }
  uint8_t ram[128] = {};
  uint8_t m_PC = 0, m_PA = 0, m_SI = 0, m_A = 0, m_X = 0, m_Y = 0;
  uint16_t m_SP[4] = {};
  uint8_t m_st = 1, m_zf = 0, m_cf = 0, m_vf = 0, m_sf = 0, m_if = 0;
  uint8_t m_pio = 0, m_TH = 0, m_TL = 0, m_SB = 0, m_o_output = 0;
  uint8_t m_pending_irq = 0;
  bool m_in_irq = false;
  int m_TP = 0, m_icount = 0, clock_remainder = 0;
  unsigned m_SBcount = 0;
};

#endif
