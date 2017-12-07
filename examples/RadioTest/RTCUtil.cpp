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
#endif


#ifdef FEATURE_LORA



#if defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_ARCH_SAMD)
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

extern void RTCInit(const char *date, const char *timestr)
{
  rtc.begin();
  rtc.attachInterrupt(alarmMatch);
  alarmMatch();
  time_t t = cvt_date(date, timestr);
  if (rtc.getYear() == 0) {
    rtc.setEpoch(t);
  }
  setTickerStartSecs(rtc.getSeconds() + (rtc.getMinutes()*60) + rtc.getHours() * 3600);
  dprintf("RTC Clock: %d/%d/%d %02d:%02d:%02d", rtc.getDay(), rtc.getMonth(), rtc.getYear() + 2000, rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
}

#elif ARDUINO_ARCH_ESP32 

RTC_DATA_ATTR int bootCount = 0;


extern void RTCInit(const char *date, const char *timestr)
{
  if (!time(NULL)) {
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
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason)
    dprintf("Boot: %s (bootCount: %d)", ESP32WakeUpReason(wakeup_reason), ++bootCount);
  dprintf("Boot: CPU(Pro): %s", ESP32ResetReason(rtc_get_reset_reason(0)));
  dprintf("Boot: CPU(App): %s", ESP32ResetReason(rtc_get_reset_reason(1)));
  dprintf("ESP32: Revision: %d (%d MHz)", ESP.getChipRevision(), ESP.getCpuFreqMHz());
}

#else
#error "Unkown platform"
#endif
#endif // FEATURE_LORA
#endif // ARDUINO 

