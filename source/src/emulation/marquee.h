#ifndef GALAGINO_MARQUEE_H
#define GALAGINO_MARQUEE_H

#include <Arduino.h>
#include <driver/spi_master.h>
#include "../config.h"

#ifdef MARQUEE_ENABLED

class Marquee {
public:
  Marquee();
  void begin();
  void select(unsigned char machineType); // stable MCH_* ID, independent of menu order
  void clear();                     // fill panel with black
  void showImage(const unsigned short *img);  // stream a 76x284 RGB565 array

private:
  void setAddrWindow();
  void sendCommand(uint8_t commandByte, const uint8_t *dataBytes, uint8_t numDataBytes);
  void writeCommand(uint8_t cmd);
  void write8(uint8_t u8);
  void write16(uint16_t u16);

  spi_device_handle_t handle;
  spi_transaction_t transaction;
  unsigned char *dma_buffer;   // DMA-capable chunk buffer
  unsigned char currentIdx = 255;  // force first select() to draw
};

#endif // MARQUEE_ENABLED
#endif // GALAGINO_MARQUEE_H
