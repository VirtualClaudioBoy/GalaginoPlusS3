#include "Arduino.h"
#include "emulation.h"
#include "../machines/machineBase.h"

extern machineBase *currentMachine;
TaskHandle_t emulationTaskHandle;
volatile static char doDeleteEmulationTask;

void emulation_start() {
#ifdef DEBUG_TIMING
  timeTotal = millis();
  cpuSum = 0;
  videoSum = 0;
  counter = 0;
#endif
  currentMachine->reset();
  currentMachine->start();
  xTaskCreatePinnedToCore(emulation_task, "emulation task", 4096, NULL, 2, &emulationTaskHandle, ARDUINO_RUNNING_CORE == 0 ? 1 : 0);
}

void emulation_stop() {
  if (!emulationTaskHandle)
    return;
  
  doDeleteEmulationTask = 1;  
  while (doDeleteEmulationTask) {
    xTaskNotifyGive(emulationTaskHandle);
    vTaskDelay(1);
  }

  emulationTaskHandle = NULL;
  currentMachine->reset();  // clear sound output
}

void emulation_notifyGive() {
  if (!emulationTaskHandle)
    return;

  xTaskNotifyGive(emulationTaskHandle);
}

void emulation_videoRendered(void) {
#ifdef DEBUG_TIMING
  videoSum += millis() - cpuStart;
#endif
}

void emulation_task(void *p) {
  for(;;) {
#ifdef DEBUG_TIMING
    cpuStart = millis();
#endif

    currentMachine->run_frame();

    if (doDeleteEmulationTask) {
      doDeleteEmulationTask = 0;
      vTaskDelete(emulationTaskHandle);
    }

#ifdef DEBUG_TIMING
    cpuSum += millis() - cpuStart;

    // Average over one nominal second.  The old counter printed immediately
    // at frame zero and its "10-frames" label did not match the calculation.
    if (++counter >= 60) {
      unsigned long now = millis();
      const unsigned long elapsed = now - timeTotal;
      const unsigned long fps100 = elapsed ? 6000000UL / elapsed : 0;
      const unsigned long cpu100 = cpuSum * 100UL / 60UL;
      printf("Game: %s | FPS: %lu.%02lu | 60 frames: %lu ms | CPU: %lu.%02lu ms/frame | Video: %lu ms\n",
             currentMachine->hiscoreKey(), fps100 / 100, fps100 % 100, elapsed,
             cpu100 / 100, cpu100 % 100, videoSum);
      timeTotal = now;
      cpuSum = 0;
      videoSum = 0;
      counter = 0;
    }
#endif

    // Wait for signal from video task to emulate a 60Hz frame rate. Don't do
    // this unless the game has actually started to speed up the boot process
    // a little bit.
    if(currentMachine->game_started)   
      ulTaskNotifyTake(1, portMAX_DELAY);
    else
      vTaskDelay(1); // give a millisecond delay to make the watchdog happy
  }
}

unsigned char OpZ80_INL(unsigned short Addr) {
  return currentMachine->opZ80(Addr);
}

void OutZ80(unsigned short Port, unsigned char Value) {
  currentMachine->outZ80(Port, Value);
}
  
unsigned char InZ80(unsigned short Port) {
  return currentMachine->inZ80(Port);
}

void WrZ80(unsigned short Addr, unsigned char Value) {
  currentMachine->wrZ80(Addr, Value);
}

unsigned char RdZ80(unsigned short Addr) {
  return currentMachine->rdZ80(Addr);
}

void PatchZ80(Z80 *R) {
}

void i8048_port_write(i8048_state_S *state, unsigned char port, unsigned char pos) {
  currentMachine->wrI8048_port(state, port, pos);
}

unsigned char i8048_port_read(i8048_state_S *state, unsigned char port) {
  return currentMachine->rdI8048_port(state, port);
}

unsigned char i8048_rom_read(i8048_state_S *state, unsigned short addr) {
  return currentMachine->rdI8048_rom(state, addr);
}

unsigned char i8048_xdm_read(i8048_state_S *state, unsigned char addr) {
  return currentMachine->rdI8048_xdm(state, addr);
}

void i8048_xdm_write(i8048_state_S *state, unsigned char addr, unsigned char data) {
}

unsigned char IRAM_ATTR m6809_read(m6809_state *s, uint16_t addr) {
  return currentMachine->m6809_read(s, addr);
}

void IRAM_ATTR m6809_write(m6809_state *s, uint16_t addr, uint8_t val) {
  currentMachine->m6809_write(s, addr, val);
}

unsigned char IRAM_ATTR m6809_read_opcode(m6809_state *s, uint16_t addr) {
  return currentMachine->m6809_read_opcode(s, addr);
}
