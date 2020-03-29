/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifndef __RADIOSTATUS_H__
#define __RADIOSTATUS_H__

#if defined(ARDUINO_Heltec_WIFI_LoRa_32) || defined(ARDUINO_WIFI_LORA_32) \
	 || defined(ARDUINO_WIFI_LORA_32_V2) || defined(ARDUINO_WIRELESS_STICK) \
	 || defined(ARDUINO_ESP32_DEV) // the Heltec and ECO boards
#define HAS_OLED_DISPLAY
#include <Wire.h>
#include "SSD1306.h"
#endif

#ifdef ARDUINO_ESP32_DEV
#undef HAS_OLED_DISPLAY // remove this line to enable the board
#endif

#ifdef FEATURE_SSD1306
#include "SSD1306I2C.h"
#define HAS_OLED_DISPLAY
#define SSD1306 SSD1306I2C
#endif

#ifndef UNUSED
 #define UNUSED(x) (void)(x)
#endif


class MyRadioStatus : public RadioStatusInterface {
public:
   	MyRadioStatus();
    virtual ~MyRadioStatus();
    
    virtual	void TXStart(int AppID, int toStation, int length, int dBm);
    virtual void TXComplete(void);
    virtual void RxDone(int size, int rssi, int snr);
    virtual void RxCompleted(void);
    virtual void MessageTimeout(int AppID, int toStation);
    
    void UpdateDisplay(bool invert);
private:
    DigitalOut *ledTX;
    DigitalOut *ledRX;
    DigitalOut *ledTimeout;
    int _totalTX;
    int _totalRX;
    int _totalError;
    int _totalTimeout;
    bool inverted;
    
#ifdef HAS_OLED_DISPLAY
    SSD1306 *display;
    DigitalOut *displayReset;
    char _line1[64];
    char _line2[64];
    char _line3[64];
    char _line4[64];
    char _line5[64];
    bool invertedDisplay;
#endif
};

#endif // __RADIOSTATUS_H__
