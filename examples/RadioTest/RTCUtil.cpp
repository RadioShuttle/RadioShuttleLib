/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifdef ARDUINO 

#include "PinMap.h"
#include <arduino-mbed.h>
#include <arduino-util.h>
#if defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_ARCH_SAMD)
#include <RTCZero.h>
#elif ARDUINO_ARCH_ESP32
#include <rom/rtc.h>
#include <driver/adc.h>
#if defined (FEATURE_SI7021) || defined (FEATURE_RTC_DS3231)
 #include <Wire.h>
#endif
#ifdef FEATURE_RTC_DS3231
 #include "ds3231.h"
#endif
#ifdef FEATURE_SI7021
  #include "Adafruit_Si7021.h"
#endif
#endif


#ifdef FEATURE_LORA

bool ESP32DeepsleepWakeup;

#if defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_ARCH_SAMD)

#ifdef BOOSTER_EN50
DigitalOut boost50(BOOSTER_EN50);
#endif
#ifdef BOOSTER_EN33
DigitalOut boost33(BOOSTER_EN33);
#endif
#ifdef DISPLAY_EN
DigitalOut displayEnable(DISPLAY_EN);
#endif

RTCZero rtc;

void alarmMatch()
{
  int interval = 5;
  int secs = rtc.getSeconds() + interval;
  if (secs >= 58)
    secs = interval;
  rtc.setAlarmSeconds(secs);
  rtc.enableAlarm(rtc.MATCH_SS);
}

void RTCInit(const char *date, const char *timestr)
{
  rtc.begin();
  rtc.attachInterrupt(alarmMatch);
  alarmMatch();
  time_t t = cvt_date(date, timestr);
  if (rtc.getYear() == 0 || rtc.getEpoch() < (uint32_t)t) {
    rtc.setEpoch(t);
  }
  setTickerStartSecs(rtc.getSeconds() + (rtc.getMinutes()*60) + rtc.getHours() * 3600);
  dprintf("RTC Clock: %d/%d/%d %02d:%02d:%02d", rtc.getDay(), rtc.getMonth(), rtc.getYear() + 2000, rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());

  uint8_t reason = PM->RCAUSE.reg;
  if (reason & (PM_RCAUSE_WDT | PM_RCAUSE_BOD33)) {
    dprintf("Boot: CPU(D21) reset by: %s %s",
      PM->RCAUSE.reg & PM_RCAUSE_WDT ? "WatchDog" : "",
      PM->RCAUSE.reg & PM_RCAUSE_BOD33 ? "Brown-Out" : "");
  }
#ifdef BOOSTER_EN50
  boost50 = 0;
  boost33 = 0;
#endif
#ifdef DISPLAY_EN
  displayEnable = 1; // disconnects the display from the 3.3 power
#endif
  InitWatchDog();
}

#elif ARDUINO_ARCH_ESP32 

#ifdef FEATURE_SI7021
Adafruit_Si7021 *sensorSI7021;  
#endif

#ifdef ESP32_ECO_POWER_REV_1
float GetBatteryVoltage()
{
#ifdef EXT_POWER_SW
  DigitalOut extPWR(EXT_POWER_SW);
  extPWR = EXT_POWER_ON;
#endif
  float volt;
  float vref = (float)prop.GetProperty(prop.ADC_VREF, 1100)/1000.0;
  
  wait_ms(1);
  analogSetPinAttenuation(BAT_POWER_ADC, ADC_0db); //  1.124 Volt ID 132, 
  wait_ms(20);  
  
  int value = 0;
  for (int i = 0; i < 12; i++) {
    value += analogRead(BAT_POWER_ADC); // BAT_POWER_ADC is ADC1
  }
  value /= 12;
  float voltstep = vref/(float)(1<<12); // 12 bit 4096
  volt = (float) ((value * voltstep) / 82) * (82+220); // 82k, 82k+220k
  volt -= 0.065; // correction in millivolt
  dprintf("Power: %.2fV (ADC: %d Vref: %.3f)", volt, value, vref); 

#ifdef EXT_POWER_SW
  extPWR = EXT_POWER_OFF; 
  DigitalIn tmpPWR(EXT_POWER_SW);
  tmpPWR.mode(PullUp); // turn off the power, PullUp will keep it off in deepsleep
#endif
  return volt;
}

#endif

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool hasSensor;


void RTCInit(const char *date, const char *timestr)
{
#if defined (FEATURE_SI7021) || defined (FEATURE_RTC_DS3231)
  Wire.begin();
#endif

  time_t now = time(NULL);
  
#ifdef FEATURE_RTC_DS3231
  DS3231_init(DS3231_CONTROL_INTCN);
  struct ts ds;
  DS3231_get(&ds);
  if (!now || now < ds.unixtime || now > ds.unixtime) {
    time_t t = cvt_date(date, timestr);
    if (!ds.unixtime || t > ds.unixtime) {
      struct tm *tmp = gmtime(&t);
      ds.sec = tmp->tm_sec;
      ds.min = tmp->tm_min;
      ds.hour = tmp->tm_hour;
      ds.wday = tmp->tm_wday;
      ds.mday = tmp->tm_mday;
      ds.mon = tmp->tm_mon + 1;
      ds.year =  tmp->tm_year + 1900;
      DS3231_set(ds);
      DS3231_get(&ds);    
#if 0  
      dprintf("%02d:%02d:%02d wday(%d) mday(%d) mon(%d) year(%d), ", ds.hour, ds.min, ds.sec, ds.wday, ds.mday, ds.mon, ds.year);
      dprintf("RTC: %.2f°C", DS3231_get_treg());
      time_t now = ds.unixtime;
      dprintf("ctime: %s", ctime(&now));
#endif
    }
    struct timeval tv;
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = ds.unixtime;;
    struct timezone tz;
    memset(&tz, 0, sizeof(tz));
    tz.tz_minuteswest = 0;
    tz.tz_dsttime = 0;
    settimeofday(&tv, &tz);    
    dprintf("RTC Clock: %d/%d/%d %02d:%02d:%02d", ds.mday, ds.mon, ds.year, ds.hour, ds.min, ds.sec);
  }
  int i = prop.GetProperty(prop.RTC_AGING_CAL, 0);
  if (i && i != DS3231_get_aging())
    DS3231_set_aging(i);
#else // without ds3231
  if (!now)) {
    time_t t = cvt_date(date, timestr);
    struct timeval tv;
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = t;
    struct timezone tz;
    memset(&tz, 0, sizeof(tz));
    tz.tz_minuteswest = 0;
    tz.tz_dsttime = 0;
    settimeofday(&tv, &tz);
  }
#endif

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason)
    dprintf("Boot: %s (bootCount: %d)", ESP32WakeUpReason(wakeup_reason), ++bootCount);
  dprintf("Boot: CPU(Pro): %s", ESP32ResetReason(rtc_get_reset_reason(0)));
  dprintf("Boot: CPU(App): %s", ESP32ResetReason(rtc_get_reset_reason(1)));
  dprintf("ESP32: Revision: %d (%d MHz)", ESP.getChipRevision(), ESP.getCpuFreqMHz());

  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) { 
    /*
     * Run this only on reset boot, not from deepsleep.
     */
#ifdef EXT_POWER_SW
    DigitalIn extPWR(EXT_POWER_SW);
    extPWR.mode(PullUp); // turn off the power, PullUp will keep it off in deepsleep
#endif
#ifdef BAT_POWER_ADC
    GetBatteryVoltage(); 
#endif

#ifdef FEATURE_SI7021
  sensorSI7021 = new Adafruit_Si7021();
  if (sensorSI7021->begin()) {
      hasSensor = true;
      dprintf("%s: Rev(%d)  %.2f°C  Humidity: %.2f%%", sensorSI7021->model, sensorSI7021->revision, sensorSI7021->readTemperature(), sensorSI7021->readHumidity());
  } else {
    delete sensorSI7021;
    sensorSI7021 = NULL;
  }
#endif
  } else {
    /* 
     *  runs on deepsleep wakeup
     */
     ESP32DeepsleepWakeup = true;
 #ifdef FEATURE_SI7021
    if (hasSensor) {
       sensorSI7021 = new Adafruit_Si7021();
       dprintf("%s: %.2f°C  Humidity: %.2f%%", sensorSI7021->model, sensorSI7021->readTemperature(), sensorSI7021->readHumidity());
     }
#endif
  }
}


#else
#error "Unkown platform"
#endif
#endif // FEATURE_LORA
#endif // ARDUINO 

