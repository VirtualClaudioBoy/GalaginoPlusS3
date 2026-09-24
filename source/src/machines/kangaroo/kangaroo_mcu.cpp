// license:BSD-3-Clause
// copyright-holders:Ernesto Corvi
// MB88xx instruction execution adapted from MAME, with Kangaroo-specific I/O.
// Source: https://github.com/mamedev/mame/blob/master/src/devices/cpu/mb88xx/mb88xx.cpp
// See LICENSE.MAME. Serial/external clock pins are unconnected on this board.
#include "kangaroo_mcu.h"
#include "kangaroo_rom.h"
#include <string.h>

static uint8_t mcu_program[0x800], protection_rom[0x200];

#define READOP(a)           (mcu_program[(a) & 0x7ff])

#define RDMEM(a)            (ram[(a) & 0x7f] & 0x0f)
#define WRMEM(a,v)          (ram[(a) & 0x7f] = (v) & 0x0f)

#define TEST_ST()           (m_st & 1)
#define TEST_ZF()           (m_zf & 1)
#define TEST_CF()           (m_cf & 1)
#define TEST_VF()           (m_vf & 1)
#define TEST_SF()           (m_sf & 1)
#define TEST_IF()           (m_if & 1)

#define UPDATE_ST_C(v)      m_st = (v & 0x10) ? 0 : 1
#define UPDATE_ST_Z(v)      m_st = (v == 0) ? 0 : 1

#define UPDATE_CF(v)        m_cf = ((v & 0x10) == 0) ? 0 : 1
#define UPDATE_ZF(v)        m_zf = (v != 0) ? 0 : 1

#define GETPC()             (((int)m_PA << 6) + m_PC)
#define GETEA()             ((m_X << 4) + m_Y)

#define INCPC()             do { m_PC++; if (m_PC >= 0x40) { m_PC = 0; m_PA++; } } while (0)



void kangaroo_mcu::reset() {
  *this = kangaroo_mcu();
  memcpy(mcu_program, kangaroo_mcu_rom, sizeof(mcu_program));
  memcpy(protection_rom, kangaroo_prot_rom, sizeof(protection_rom));
}

void kangaroo_mcu::write_pla(uint8_t index) {
  const unsigned shift = (index & 0x10) ? 4 : 0;
  const uint8_t mask = 15 << shift;
  m_o_output = (m_o_output & ~mask) | ((index << shift) & mask);
}

void kangaroo_mcu::write_r(unsigned port, uint8_t value) {
  if (port == 3 && !(ports[3] & 8) && (value & 8)) nmi_pending = true;
  ports[port] = value & 15;
}

uint8_t kangaroo_mcu::read_k() {
  uint8_t value = 15;
  if (ports[2] & 1) value &= data;
  if (!(ports[3] & 1)) value &= protection_rom[m_o_output | ((ports[3] & 2) << 7)];
  return value;
}

void kangaroo_mcu::run(int clocks) {
  // One MCU instruction cycle consumes six input clock periods.
  clock_remainder += clocks;
  m_icount += clock_remainder / 6;
  clock_remainder %= 6;
  execute_run();
}

void kangaroo_mcu::increment_timer() {
  m_TL = (m_TL + 1) & 15;
  if (!m_TL) {
    m_TH = (m_TH + 1) & 15;
    if (!m_TH) { m_vf = 1; m_pending_irq |= 2; }
  }
}

void kangaroo_mcu::burn_cycles(int cycles) {
  m_icount -= cycles;
  // MAME's serial clock divider and the instruction divider are both six.
  if ((m_pio & 0x30) == 0x20 && m_SBcount < 1000) {
    for (int i = 0; i < cycles && m_SBcount < 1000; i++) {
      m_SBcount++;
      if (!m_sf) {
        m_SB >>= 1; // SI is unconnected (zero).
        if (m_SBcount >= 4) { m_sf = 1; m_pending_irq |= 1; }
      }
    }
  }
  if (m_pio & 0x80) {
    m_TP += cycles;
    while (m_TP >= 32) { m_TP -= 32; increment_timer(); }
  }
  if (!m_in_irq && (m_pending_irq & m_pio)) {
    m_in_irq = true;
    m_SP[m_SI] = GETPC() | (TEST_CF() << 15) | (TEST_ZF() << 14) | (TEST_ST() << 13);
    m_SI = (m_SI + 1) & 3;
    m_PC = (m_pending_irq & m_pio & 2) ? 4 : 6;
    m_PA = 0;
    m_st = 1;
    m_pending_irq = 0;
    burn_cycles(3);
  }
}

void kangaroo_mcu::execute_run()
{
	while (m_icount > 0)
	{
		uint8_t opcode, arg, oc;

		// fetch the opcode
		opcode = READOP(GETPC());

		// increment the PC
		INCPC();

		// start with instruction doing 1 cycle
		oc = 1;

		switch (opcode)
		{
			case 0x00: // nop ZCS:...
				m_st = 1;
				break;

			case 0x01: // outO ZCS:...
				write_pla(TEST_CF() << 4 | m_A);
				m_st = 1;
				break;

			case 0x02: // outP ZCS:...
				// Port P is not connected on Kangaroo.
				m_st = 1;
				break;

			case 0x03: // outR ZCS:...
				arg = m_Y;
				write_r(arg & 3, m_A);
				m_st = 1;
				break;

			case 0x04: // tay ZCS:...
				m_Y = m_A;
				m_st = 1;
				break;

			case 0x05: // tath ZCS:...
				m_TH = m_A;
				m_st = 1;
				break;

			case 0x06: // tatl ZCS:...
				m_TL = m_A;
				m_st = 1;
				break;

			case 0x07: // tas ZCS:...
				m_SB = m_A;
				m_st = 1;
				break;

			case 0x08: // icy ZCS:x.x
				m_Y++;
				UPDATE_ST_C(m_Y);
				m_Y &= 0x0f;
				UPDATE_ZF(m_Y);
				break;

			case 0x09: // icm ZCS:x.x
				arg = RDMEM(GETEA());
				arg++;
				UPDATE_ST_C(arg);
				arg &= 0x0f;
				UPDATE_ZF(arg);
				WRMEM(GETEA(), arg);
				break;

			case 0x0a: // stic ZCS:x.x
				WRMEM(GETEA(), m_A);
				m_Y++;
				UPDATE_ST_C(m_Y);
				m_Y &= 0x0f;
				UPDATE_ZF(m_Y);
				break;

			case 0x0b: // x ZCS:x..
				arg = RDMEM(GETEA());
				WRMEM(GETEA(), m_A);
				m_A = arg;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x0c: // rol ZCS:xxx
				m_A <<= 1;
				m_A |= TEST_CF();
				UPDATE_ST_C(m_A);
				m_cf = m_st ^ 1;
				m_A &= 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x0d: // l ZCS:x..
				m_A = RDMEM(GETEA());
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x0e: // adc ZCS:xxx
				arg = RDMEM(GETEA());
				arg += m_A;
				arg += TEST_CF();
				UPDATE_ST_C(arg);
				m_cf = m_st ^ 1;
				m_A = arg & 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x0f: // and ZCS:x.x
				m_A &= RDMEM(GETEA());
				UPDATE_ZF(m_A);
				m_st = m_zf ^ 1;
				break;

			case 0x10: // daa ZCS:.xx
				if (TEST_CF() || m_A > 9) m_A += 6;
				UPDATE_ST_C(m_A);
				m_cf = m_st ^ 1;
				m_A &= 0x0f;
				break;

			case 0x11: // das ZCS:.xx
				if (TEST_CF() || m_A > 9) m_A += 10;
				UPDATE_ST_C(m_A);
				m_cf = m_st ^ 1;
				m_A &= 0x0f;
				break;

			case 0x12: // inK ZCS:x..
				m_A = read_k() & 0x0f;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x13: // inR ZCS:x..
				arg = m_Y;
				m_A = ports[arg & 3] & 0x0f;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x14: // tya ZCS:x..
				m_A = m_Y;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x15: // ttha ZCS:x..
				m_A = m_TH;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x16: // ttla ZCS:x..
				m_A = m_TL;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x17: // tsa ZCS:x..
				m_A = m_SB;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x18: // dcy ZCS:..x
				m_Y--;
				UPDATE_ST_C(m_Y);
				m_Y &= 0x0f;
				break;

			case 0x19: // dcm ZCS:x.x
				arg = RDMEM(GETEA());
				arg--;
				UPDATE_ST_C(arg);
				arg &= 0x0f;
				UPDATE_ZF(arg);
				WRMEM(GETEA(), arg);
				break;

			case 0x1a: // stdc ZCS:x.x
				WRMEM(GETEA(), m_A);
				m_Y--;
				UPDATE_ST_C(m_Y);
				m_Y &= 0x0f;
				UPDATE_ZF(m_Y);
				break;

			case 0x1b: // xx ZCS:x..
				arg = m_X;
				m_X = m_A;
				m_A = arg;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x1c: // ror ZCS:xxx
				m_A |= TEST_CF() << 4;
				UPDATE_ST_C(m_A << 4);
				m_cf = m_st ^ 1;
				m_A >>= 1;
				m_A &= 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x1d: // st ZCS:x..
				WRMEM(GETEA(), m_A);
				m_st = 1;
				break;

			case 0x1e: // sbc ZCS:xxx
				arg = RDMEM(GETEA());
				arg -= m_A;
				arg -= TEST_CF();
				UPDATE_ST_C(arg);
				m_cf = m_st ^ 1;
				m_A = arg & 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x1f: // or ZCS:x.x
				m_A |= RDMEM(GETEA());
				UPDATE_ZF(m_A);
				m_st = m_zf ^ 1;
				break;

			case 0x20: // setR ZCS:...
				arg = ports[m_Y >> 2] & 0x0f;
				write_r(m_Y >> 2, arg | (1 << (m_Y & 3)));
				m_st = 1;
				break;

			case 0x21: // setc ZCS:.xx
				m_cf = 1;
				m_st = 1;
				break;

			case 0x22: // rstR ZCS:...
				arg = ports[m_Y >> 2] & 0x0f;
				write_r(m_Y >> 2, arg & ~(1 << (m_Y & 3)));
				m_st = 1;
				break;

			case 0x23: // rstc ZCS:.xx
				m_cf = 0;
				m_st = 1;
				break;

			case 0x24: // tstr ZCS:..x
				arg = ports[m_Y >> 2] & 0x0f;
				m_st = (arg & (1 << (m_Y & 3))) ? 0 : 1;
				break;

			case 0x25: // tsti ZCS:..x
				m_st = m_if ^ 1;
				break;

			case 0x26: // tstv ZCS:..x
				m_st = m_vf ^ 1;
				m_vf = 0;
				break;

			case 0x27: // tsts ZCS:..x
				m_st = m_sf ^ 1;
				if (m_sf) m_SBcount = 0;
				m_sf = 0;
				break;

			case 0x28: // tstc ZCS:..x
				m_st = m_cf ^ 1;
				break;

			case 0x29: // tstz ZCS:..x
				m_st = m_zf ^ 1;
				break;

			case 0x2a: // sts ZCS:x..
				WRMEM(GETEA(), m_SB);
				UPDATE_ZF(m_SB);
				m_st = 1;
				break;

			case 0x2b: // ls ZCS:x..
				m_SB = RDMEM(GETEA());
				UPDATE_ZF(m_SB);
				m_st = 1;
				break;

			case 0x2c: // rts ZCS:...
				m_SI = (m_SI - 1) & 3;
				m_PC = m_SP[m_SI] & 0x3f;
				m_PA = (m_SP[m_SI] >> 6) & 0x1f;
				m_st = 1;
				break;

			case 0x2d: // neg ZCS: ..x
				m_A = (~m_A) + 1;
				m_A &= 0x0f;
				UPDATE_ST_Z(m_A);
				break;

			case 0x2e: // c ZCS:xxx
				arg = RDMEM(GETEA());
				arg -= m_A;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				m_zf = m_st ^ 1;
				break;

			case 0x2f: // eor ZCS:x.x
				m_A ^= RDMEM(GETEA());
				UPDATE_ST_Z(m_A);
				m_zf = m_st ^ 1;
				break;

			case 0x30: case 0x31: case 0x32: case 0x33: // sbit ZCS:...
				arg = RDMEM(GETEA());
				WRMEM(GETEA(), arg | (1 << (opcode & 3)));
				m_st = 1;
				break;

			case 0x34: case 0x35: case 0x36: case 0x37: // rbit ZCS:...
				arg = RDMEM(GETEA());
				WRMEM(GETEA(), arg & ~(1 << (opcode & 3)));
				m_st = 1;
				break;

			case 0x38: case 0x39: case 0x3a: case 0x3b: // tbit ZCS:...
				arg = RDMEM(GETEA());
				m_st = (arg & (1 << (opcode & 3))) ? 0 : 1;
				break;

			case 0x3c: // rti ZCS:...
				// restore address and saved state flags on the top bits of the stack
				m_in_irq = false;
				m_SI = (m_SI - 1) & 3;
				m_PC = m_SP[m_SI] & 0x3f;
				m_PA = (m_SP[m_SI] >> 6) & 0x1f;
				m_st = (m_SP[m_SI] >> 13) & 1;
				m_zf = (m_SP[m_SI] >> 14) & 1;
				m_cf = (m_SP[m_SI] >> 15) & 1;
				break;

			case 0x3d: // jpa imm ZCS:..x
				m_PA = READOP(GETPC()) & 0x1f;
				m_PC = m_A * 4;
				oc++;
				m_st = 1;
				break;

			case 0x3e: // en imm ZCS:...
				pio_enable(m_pio | READOP(GETPC()));
				INCPC();
				oc++;
				m_st = 1;
				break;

			case 0x3f: // dis imm ZCS:...
				pio_enable(m_pio & ~(READOP(GETPC())));
				INCPC();
				oc++;
				m_st = 1;
				break;

			case 0x40: case 0x41: case 0x42: case 0x43: // setD ZCS:...
				arg = ports[0] & 0x0f;
				arg |= (1 << (opcode & 3));
				write_r(0, arg);
				m_st = 1;
				break;

			case 0x44: case 0x45: case 0x46: case 0x47: // rstD ZCS:...
				arg = ports[0] & 0x0f;
				arg &= ~(1 << (opcode & 3));
				write_r(0, arg);
				m_st = 1;
				break;

			case 0x48: case 0x49: case 0x4a: case 0x4b: // tstD ZCS:..x
				arg = ports[2] & 0x0f;
				m_st = (arg & (1 << (opcode & 3))) ? 0 : 1;
				break;

			case 0x4c: case 0x4d: case 0x4e: case 0x4f: // tba ZCS:..x
				m_st = (m_A & (1 << (opcode & 3))) ? 0 : 1;
				break;

			case 0x50: case 0x51: case 0x52: case 0x53: // xd ZCS:x..
				arg = RDMEM(opcode&3);
				WRMEM((opcode & 3), m_A);
				m_A = arg;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x54: case 0x55: case 0x56: case 0x57: // xyd ZCS:x..
				arg = RDMEM((opcode & 3) + 4);
				WRMEM((opcode & 3) + 4, m_Y);
				m_Y = arg;
				UPDATE_ZF(m_Y);
				m_st = 1;
				break;

			case 0x58: case 0x59: case 0x5a: case 0x5b:
			case 0x5c: case 0x5d: case 0x5e: case 0x5f: // lxi ZCS:x..
				m_X = opcode & 7;
				UPDATE_ZF(m_X);
				m_st = 1;
				break;

			case 0x60: case 0x61: case 0x62: case 0x63:
			case 0x64: case 0x65: case 0x66: case 0x67: // call imm ZCS:..x
				arg = READOP(GETPC());
				INCPC();
				oc++;
				if (TEST_ST())
				{
					m_SP[m_SI] = GETPC();
					m_SI = (m_SI + 1) & 3;
					m_PC = arg & 0x3f;
					m_PA = ((opcode & 7) << 2) | (arg >> 6);
				}
				m_st = 1;
				break;

			case 0x68: case 0x69: case 0x6a: case 0x6b:
			case 0x6c: case 0x6d: case 0x6e: case 0x6f: // jpl imm ZCS:..x
				arg = READOP(GETPC());
				INCPC();
				oc++;
				if (TEST_ST())
				{
					m_PC = arg & 0x3f;
					m_PA = ((opcode & 7) << 2) | (arg >> 6);
				}
				m_st = 1;
				break;

			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
			case 0x78: case 0x79: case 0x7a: case 0x7b:
			case 0x7c: case 0x7d: case 0x7e: case 0x7f: // ai ZCS:xxx
				arg = opcode & 0x0f;
				arg += m_A;
				UPDATE_ST_C(arg);
				m_cf = m_st ^ 1;
				m_A = arg & 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x80: case 0x81: case 0x82: case 0x83:
			case 0x84: case 0x85: case 0x86: case 0x87:
			case 0x88: case 0x89: case 0x8a: case 0x8b:
			case 0x8c: case 0x8d: case 0x8e: case 0x8f: // lxi ZCS:x..
				m_Y = opcode & 0x0f;
				UPDATE_ZF(m_Y);
				m_st = 1;
				break;

			case 0x90: case 0x91: case 0x92: case 0x93:
			case 0x94: case 0x95: case 0x96: case 0x97:
			case 0x98: case 0x99: case 0x9a: case 0x9b:
			case 0x9c: case 0x9d: case 0x9e: case 0x9f: // li ZCS:x..
				m_A = opcode & 0x0f;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0xa0: case 0xa1: case 0xa2: case 0xa3:
			case 0xa4: case 0xa5: case 0xa6: case 0xa7:
			case 0xa8: case 0xa9: case 0xaa: case 0xab:
			case 0xac: case 0xad: case 0xae: case 0xaf: // cyi ZCS:xxx
				arg = (opcode & 0x0f) - m_Y;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				m_zf = m_st ^ 1;
				break;

			case 0xb0: case 0xb1: case 0xb2: case 0xb3:
			case 0xb4: case 0xb5: case 0xb6: case 0xb7:
			case 0xb8: case 0xb9: case 0xba: case 0xbb:
			case 0xbc: case 0xbd: case 0xbe: case 0xbf: // ci ZCS:xxx
				arg = (opcode & 0x0f) - m_A;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				m_zf = m_st ^ 1;
				break;

			default: // jmp ZCS:..x
				if (TEST_ST())
					m_PC = opcode & 0x3f;
				m_st = 1;
				break;
		}

		// update cycle count, also update interrupts, serial and timer flags
		burn_cycles(oc);
	}
}
