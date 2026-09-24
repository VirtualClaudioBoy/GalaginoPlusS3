#include "marquee.h"

#ifdef MARQUEE_ENABLED
#include "../machines/machineBase.h"
#include "marquee_images.h"

// ST7789 native controller resolution in landscape (MADCTL 0x60): 320x240.
// The physical visible area of this panel is 284x76, located at
// colstart = 18, rowstart = 82. We keep the full 320x240 framebuffer
// window and place the 284x76 image at those offsets (as suggested by
// TFT_eSPI PR #3769).
#define MARQUEE_CTRL_W      320
#define MARQUEE_CTRL_H      240
#define MARQUEE_ROW_BYTES   (MARQUEE_CTRL_W * 2)   // 640 bytes per row

static spi_device_interface_config_t marquee_if_cfg {
  .command_bits = 0,
  .address_bits = 0,
  .dummy_bits = 0,
  .mode = SPI_MODE0,
  .duty_cycle_pos = 128,
  .cs_ena_pretrans = 0,
  .cs_ena_posttrans = 0,
  .clock_speed_hz = MARQUEE_SPICLK,
  .input_delay_ns = 0,
  .spics_io_num = -1,        // CS handled manually (like main display in video.cpp)
  .flags = SPI_DEVICE_HALFDUPLEX,
  .queue_size = 3,
  .pre_cb = NULL,
  .post_cb = NULL,
};

static spi_bus_config_t marquee_bus_cfg{
    .mosi_io_num = MARQUEE_MOSI,
    .miso_io_num = -1,              // no MISO for ST7789
    .sclk_io_num = MARQUEE_SCLK,
    .max_transfer_sz = MARQUEE_ROW_BYTES,
    .flags = SPICOMMON_BUSFLAG_MASTER,
};

Marquee::Marquee() {
  memset(&transaction, 0, sizeof(transaction));
  pinMode(MARQUEE_CS, OUTPUT);
  digitalWrite(MARQUEE_CS, HIGH); // deselect
  pinMode(MARQUEE_DC, OUTPUT);
  digitalWrite(MARQUEE_DC, HIGH);

  dma_buffer = (unsigned char*)heap_caps_malloc(MARQUEE_ROW_BYTES, MALLOC_CAP_DMA);
  if (!dma_buffer)
    printf("Marquee: no DMA memory\n");

  // Use a dedicated SPI bus (SPI3) so DMA transfers on the main
  // display (SPI2) are not disturbed.
  spi_bus_initialize(SPI3_HOST, &marquee_bus_cfg, SPI_DMA_CH_AUTO);
  spi_bus_add_device(SPI3_HOST, &marquee_if_cfg, &handle);
}

void Marquee::begin() {
  // hardware reset
  if (MARQUEE_RST >= 0) {
    pinMode(MARQUEE_RST, OUTPUT);
    digitalWrite(MARQUEE_RST, LOW);
    delay(100);
    digitalWrite(MARQUEE_RST, HIGH);
    delay(200);
  }

  // ST7789 init - complete sequence (same as main display in video.cpp)
  static const uint8_t d_pwr1[]    = {0x23};                    // Power control VRH[5:0]
  static const uint8_t d_pwr2[]    = {0x10};                    // Power control SAP[2:0];BT[3:0]
  static const uint8_t d_vcm[]     = {0x3e, 0x28};              // VCM control
  static const uint8_t d_vcm2[]    = {0x86};                    // VCM control2
  static const uint8_t d_fr[]      = {0x00, 0x18};              // Framerate control
  static const uint8_t d_dfc[]     = {0x0A, 0x82, 0x27};        // Display function control
  static const uint8_t d_gamma_p[] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
                                      0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03,
                                      0x0E, 0x09, 0x00};        // Gamma +
  static const uint8_t d_gamma_n[] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
                                      0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C,
                                      0x31, 0x36, 0x0F};        // Gamma -

  sendCommand(0x01, NULL, 0);             // software reset
  delay(150);
  sendCommand(0x11, NULL, 0);             // sleep out
  delay(120);

  // Power control
  sendCommand(0xC0, d_pwr1, 1);
  sendCommand(0xC1, d_pwr2, 1);
  sendCommand(0xC5, d_vcm, 2);
  sendCommand(0xC7, d_vcm2, 1);

  // Memory access control (orientation)
  uint8_t madctl = MARQUEE_MADCTL;
  sendCommand(0x36, &madctl, 1);

  // Pixel format
  uint8_t pixfmt = 0x55;                  // 16 bpp
  sendCommand(0x3a, &pixfmt, 1);

  // Display function control
  sendCommand(0xB6, d_dfc, 3);

  // Framerate control
  sendCommand(0xB1, d_fr, 2);

  // Gamma curve
  sendCommand(0xE0, d_gamma_p, 15);
  sendCommand(0xE1, d_gamma_n, 15);

  // Inversion
#ifdef MARQUEE_INV_ON
  sendCommand(0x21, NULL, 0);             // inversion on
#else
  sendCommand(0x20, NULL, 0);             // inversion off
#endif

  sendCommand(0x13, NULL, 0);             // normal display on
  delay(10);
  sendCommand(0x29, NULL, 0);             // display on
  delay(10);

  clear();

  // BL is hard-wired to GND, no pin needed
}

void Marquee::clear() {
  setAddrWindow();
  memset(dma_buffer, 0, MARQUEE_ROW_BYTES);

  for (int y = 0; y < MARQUEE_CTRL_H; y++) {
    transaction.flags = 0;
    transaction.tx_buffer = dma_buffer;
    transaction.length = MARQUEE_ROW_BYTES * 8;
    spi_device_transmit(handle, &transaction);
  }
}

void Marquee::select(unsigned char machineType) {
  if (machineType == currentIdx)
    return;
  currentIdx = machineType;
  const unsigned short *img = nullptr;
  switch (machineType) {
    case MCH_1942: img = marquee_1942; break;
    case MCH_ALIBABA: img = marquee_alibaba; break;
    case MCH_AMIDAR: img = marquee_amidar; break;
    case MCH_ANTEATER: img = marquee_anteater; break;
    case MCH_BAGMAN: img = marquee_bagman; break;
    case MCH_BNJ: img = marquee_bnj; break;
    case MCH_BOMBJACK: img = marquee_bombjack; break;
    case MCH_BTIME: img = marquee_btime; break;
    case MCH_CIRCUSC: img = marquee_circusc; break;
    case MCH_CRUSH: img = marquee_crush; break;
    case MCH_DIGDUG: img = marquee_digdug; break;
    case MCH_DKONG: img = marquee_dkong; break;
    case MCH_DKONGJR: img = marquee_dkongjr; break;
    case MCH_DKONG3: img = marquee_dkong3; break;
    case MCH_EYES: img = marquee_eyes; break;
    case MCH_FANTASY: img = marquee_fantasy; break;
    case MCH_FROGGER: img = marquee_frogger; break;
    case MCH_GALAGA: img = marquee_galaga; break;
    case MCH_GALAXIAN: img = marquee_galaxian; break;
    case MCH_GAPLUS: img = marquee_gaplus; break;
    case MCH_GYRUSS: img = marquee_gyruss; break;
    case MCH_LADYBUG: img = marquee_ladybug; break;
    case MCH_LIZWIZ: img = marquee_lizwiz; break;
    case MCH_MAPPY: img = marquee_mappy; break;
    case MCH_MOONCRESTA: img = marquee_mooncresta; break;
    case MCH_MRDO: img = marquee_mrdo; break;
    case MCH_MRTNT: img = marquee_mrtnt; break;
    case MCH_MSPACMAN: img = marquee_mspacman; break;
    case MCH_NIBBLER: img = marquee_nibbler; break;
    case MCH_PACMAN: img = marquee_pacman; break;
    case MCH_PENGO: img = marquee_pengo; break;
    case MCH_PHOENIX: img = marquee_phoenix; break;
    case MCH_PBACTION: img = marquee_pbaction; break;
    case MCH_POOYAN: img = marquee_pooyan; break;
    case MCH_QIX: img = marquee_qix; break;
    case MCH_ROCNROPE: img = marquee_rocnrope; break;
    case MCH_SCRAMBLE: img = marquee_scramble; break;
    case MCH_SCREGG: img = marquee_scregg; break;
    case MCH_SPACE: img = marquee_space; break;
    case MCH_STARFORCE: img = marquee_starforce; break;
    case MCH_SUPERCOBRA: img = marquee_supercobra; break;
    case MCH_KANGAROO: img = marquee_kangaroo; break;
    case MCH_THEGLOB: img = marquee_theglob; break;
    case MCH_TIMEPLT: img = marquee_timeplt; break;
    case MCH_TODRUAGA: img = marquee_todruaga; break;
    case MCH_TURTLES: img = marquee_turtles; break;
    case MCH_TUTANKHM: img = marquee_tutankhm; break;
    case MCH_VANVAN: img = marquee_vanvan; break;
    case MCH_VANGUARD: img = marquee_vanguard; break;
    case MCH_XEVIOUS: img = marquee_xevious; break;
    default: break;
  }
  if (!img) {
    clear();
    return;
  }

  showImage(img);
}
/*
void Marquee::showImage(const unsigned short *img) {
  setAddrWindow();

  const unsigned short *src = img;

  for (int y = 0; y < MARQUEE_CTRL_H; y++) {
    // Blank row (above/below the 76-row visible strip)
    if (y < MARQUEE_Y_OFFSET || y >= MARQUEE_Y_OFFSET + MARQUEE_H) {
      memset(dma_buffer, 0, MARQUEE_ROW_BYTES);
    } else {
      // Image row. The left/right margins are black, image pixels are
      // placed in the middle with R<->B swapped (the panel reports
      // the GRAM in the opposite order of RGB565).
      memset(dma_buffer, 0, MARQUEE_ROW_BYTES);
      unsigned short *dst = (unsigned short *)(dma_buffer + MARQUEE_X_OFFSET * 2);
      const unsigned short *line = src + (y - MARQUEE_Y_OFFSET) * MARQUEE_W;
      for (int x = 0; x < MARQUEE_W; x++) {
        unsigned short p = line[x];
        // Convert RGB565: swap red (bits 15-11) and blue (bits 4-0)
        unsigned short r = (p >> 11) & 0x1F;
        unsigned short g = (p >> 5)  & 0x3F;
        unsigned short b =  p        & 0x1F;
        dst[x] = (b << 11) | (g << 5) | r;
      }
    }

    transaction.flags = 0;
    transaction.tx_buffer = dma_buffer;
    transaction.length = MARQUEE_ROW_BYTES * 8;
    spi_device_transmit(handle, &transaction);
  }
}
*/
void Marquee::showImage(const unsigned short *img) {
  setAddrWindow();
  const unsigned short *src = img;

  for (int y = 0; y < MARQUEE_CTRL_H; y++) {
    if (y < MARQUEE_Y_OFFSET || y >= MARQUEE_Y_OFFSET + MARQUEE_H) {
      memset(dma_buffer, 0, MARQUEE_ROW_BYTES);
    } else {
      memset(dma_buffer, 0, MARQUEE_ROW_BYTES);
      unsigned short *dst = (unsigned short *)(dma_buffer + MARQUEE_X_OFFSET * 2);
      const unsigned short *line = src + (y - MARQUEE_Y_OFFSET) * MARQUEE_W;
      for (int x = 0; x < MARQUEE_W; x++) {
        dst[x] = line[x];   // nessuna manipolazione: Python ha già l'ordine byte corretto
      }
    }

    transaction.flags = 0;
    transaction.tx_buffer = dma_buffer;
    transaction.length = MARQUEE_ROW_BYTES * 8;
    spi_device_transmit(handle, &transaction);
  }
}

void Marquee::setAddrWindow() {
  digitalWrite(MARQUEE_CS, LOW);
  writeCommand(0x2A);
  write16(0);
  write16(MARQUEE_CTRL_W - 1);
  writeCommand(0x2B);
  write16(0);
  write16(MARQUEE_CTRL_H - 1);
  writeCommand(0x2C);
  // CS is kept LOW for the following pixel data
}

void Marquee::sendCommand(uint8_t commandByte, const uint8_t *dataBytes, uint8_t numDataBytes) {
  digitalWrite(MARQUEE_CS, LOW);
  writeCommand(commandByte);
  for (int i = 0; i < numDataBytes; i++)
    write8(dataBytes[i]);
  digitalWrite(MARQUEE_CS, HIGH);
}

void Marquee::writeCommand(uint8_t cmd) {
  digitalWrite(MARQUEE_DC, LOW);
  write8(cmd);
  digitalWrite(MARQUEE_DC, HIGH);
}

void Marquee::write16(uint16_t data) {
  transaction.flags = SPI_TRANS_USE_TXDATA;
  transaction.length = 16;
  transaction.tx_data[0] = (data >> 8) & 0xFF;
  transaction.tx_data[1] = data & 0xFF;
  spi_device_transmit(handle, &transaction);
}

void Marquee::write8(uint8_t data) {
  transaction.flags = SPI_TRANS_USE_TXDATA;
  transaction.length = 8;
  transaction.tx_data[0] = data;
  spi_device_transmit(handle, &transaction);
}

#endif // MARQUEE_ENABLED
