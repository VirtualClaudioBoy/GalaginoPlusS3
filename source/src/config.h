#ifndef _CONFIG_H_
#define _CONFIG_H_

// game config
#define MASTER_ATTRACT_MENU_TIMEOUT  10000      // start games while sitting idle in menu for 20 seconds, undefine to disable
#define MASTER_ATTRACT_GAME_TIMEOUT  60000 * 2  // restart after 5 minutes

#define MASTER_ATTRACT_MUTE_AUDIO

#define AUTOFIRE_ENABLED
#define AUTOFIRE_ON_MS   40   
#define AUTOFIRE_OFF_MS  40   

// video config
#if TFT_SPICLK < 80000000
  #define VIDEO_HALF_RATE
#endif

#define TFT_X_OFFSET      8
#define TFT_Y_OFFSET      16

#define LED_BRIGHTNESS 	  50

#define SND_LEFT_CHANNEL 

#define I2S_BCLK_PIN  13
#define I2S_LRC_PIN   14
#define I2S_DIN_PIN   12
#define I2S_SD_PIN    -1   
#define I2S_GAIN_PIN  -1   

#ifdef CHEAP_YELLOW_DISPLAY_CONF
  #define TFT_CS          15
  #define TFT_DC          2
  #define TFT_RST         -1
  #define TFT_BL          27   
  #define TFT_BL_LEVEL    HIGH  
  #define TFT_MISO 	      12
  #define TFT_MOSI 	      13
  #define TFT_SCLK 	      14
  #define BTN_START_PIN	  35
  #define BTN_LEFT_PIN    21
  #define BTN_RIGHT_PIN   22
  #define BTN_DOWN_PIN    16
  #define BTN_UP_PIN      17
  #define BTN_FIRE_PIN    4
#endif

#ifndef CHEAP_YELLOW_DISPLAY_CONF

  #define MARQUEE_ENABLED

  #ifndef NUNCHUCK_INPUT
    #define TFT_MISO 	      4
    #define TFT_MOSI 	      7
    #define TFT_SCLK 	      6
    #define TFT_CS          17
    #define TFT_DC          15
    #define TFT_RST         16
    #define TFT_BL          5      
    #define TFT_BL_LEVEL    HIGH     
    #define TFT_VFLIP               
	  #define BTN_START_PIN   39
	  #define BTN_COIN_PIN    38      

    #define BTN_LEFT_PIN  41
    #define BTN_RIGHT_PIN 42
    #define BTN_DOWN_PIN  1
    #define BTN_UP_PIN    2
    #define BTN_FIRE_PIN  40
  #else
    #define TFT_MISO 	      4
    #define TFT_MOSI 	      7
    #define TFT_SCLK 	      6
    #define TFT_CS          17
    #define TFT_DC          15
    #define TFT_RST         16
    #define TFT_BL          5      
    #define TFT_BL_LEVEL    HIGH     
    #define TFT_VFLIP               
    #define NUNCHUCK_SDA  21
    #define NUNCHUCK_SCL  47
    #define NUNCHUCK_MOVE_THRESHOLD 30 
  #endif
#endif

// ---------------------------------------------------------------------------
// Marquee display (second ST7789, 284x76 landscape)
//
//   Uses a DEDICATED SPI bus (SPI3) to avoid interfering with the
//   main display (SPI2) which runs DMA transfers.
//   Backlight is hard-wired to GND (always on).
// ---------------------------------------------------------------------------
#ifdef MARQUEE_ENABLED
  #define MARQUEE_CS        18
  #define MARQUEE_DC        9
  #define MARQUEE_RST       8
  #define MARQUEE_SCLK      10        //SCL
  #define MARQUEE_MOSI      11        //SDA
  #define MARQUEE_BL        -1       // hard-wired to GND
  #define MARQUEE_BL_LEVEL  HIGH

  #define MARQUEE_W         284
  #define MARQUEE_H         76
  // 2.25" ST7789 76x284: the ST7789 controller has a native 240x320
  // resolution, this panel only uses a 76x284 window. In landscape
  // (MADCTL 0x48 = MX + BGR) the offsets are:
  //   colstart = 18 (X), rowstart = 82 (Y)
  // See https://github.com/Bodmer/TFT_eSPI/pull/3769
  #define MARQUEE_X_OFFSET  18
  #define MARQUEE_Y_OFFSET  82
  #define MARQUEE_MADCTL    0x80     // MX only (no BGR - colors are swapped in software)
  //#define MARQUEE_INV_ON  // uncomment if colors are inverted
  #define MARQUEE_SPICLK    40000000
#endif

#endif // _CONFIG_H_