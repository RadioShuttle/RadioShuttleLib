/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifdef ARDUINO 

#include "PinMap.h"
#include <arduino-mbed.h>
#include <sx1276-mbed-hal.h>
#include <RadioShuttle.h>
#include <RadioStatus.h>
#include <RadioSecurity.h>
#include <arduino-util.h>
#ifdef ARDUINO_SAMD_ZERO 
#include <sys/time.h>
#include <time.h>
#include <RTCZero.h>
#elif ARDUINO_ARCH_ESP32
#include <sys/time.h>
#include <time.h>
#include <rom/rtc.h>
#include <esp_deep_sleep.h>
#endif


#ifdef FEATURE_LORA

// Convert compile time to system time 
time_t cvt_date(char const *date, char const *time)
{
    char s_month[5];
    int year;
    struct tm t;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    sscanf(date, "%s %d %d", s_month, &t.tm_mday, &year);
    sscanf(time, "%2d %*c %2d %*c %2d", &t.tm_hour, &t.tm_min, &t.tm_sec);
    // Find where is s_month in month_names. Deduce month value.
    t.tm_mon = (strstr(month_names, s_month) - month_names) / 3;  
    t.tm_year = year - 1900;    
    return mktime(&t);
}

#ifdef ARDUINO_SAMD_ZERO 
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
    // dprintf("TIME: %s", __TIME__);
    // dprintf("DATE: %s", __DATE__);
  }
  setTickerStartSecs(rtc.getSeconds() + (rtc.getMinutes()*60) + rtc.getHours() * 3600);
  dprintf("RTC Clock: %d/%d/%d %02d:%02d:%02d", rtc.getDay(), rtc.getMonth(), rtc.getYear() + 2000, rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
  dprintf("Hours: %d", rtc.getHours() * 3600);
}

#elif ARDUINO_ARCH_ESP32 

RTC_DATA_ATTR int bootCount = 0;

static const char *reset_reason_string(RESET_REASON r)
{
  const char *reason = "";

  switch(r) {
    case NO_MEAN:
      reason = "no mean";
      break;
    case POWERON_RESET:
      reason = "Vbat power on reset";
      break;
    case SW_RESET:
      reason = "Software reset digital core";
      break;
    case OWDT_RESET:
      reason = "Legacy watch dog reset digital core";
      break;
    case DEEPSLEEP_RESET:
      reason = "Deep Sleep reset digital core";
      break;
    case SDIO_RESET:
      reason = "Reset by SLC module, reset digital core";
      break;
    case TG0WDT_SYS_RESET:
      reason = "Timer Group0 Watch dog reset digital core";
      break;
    case TG1WDT_SYS_RESET:
      reason = "Timer Group1 Watch dog reset digital core";
      break;
    case RTCWDT_SYS_RESET:
      reason = "RTC Watch dog Reset digital core";
      break;
    case INTRUSION_RESET:
      reason = "Instrusion tested to reset CPU";
      break;
    case TGWDT_CPU_RESET:
      reason = "Time Group reset CPU";
      break;
    case SW_CPU_RESET:
      reason = "Software reset CPU";
      break;
    case RTCWDT_CPU_RESET:
      reason = "RTC Watch dog Reset CPU";
      break;
    case EXT_CPU_RESET:
      reason = "APP CPU reseted by PRO CPU";
      break;
    case RTCWDT_BROWN_OUT_RESET:
      reason = "Reset when the vdd voltage is not stable";
      break;
    case RTCWDT_RTC_RESET:
      reason = "RTC Watch dog reset digital core and rtc module";
      break;
    default:
      reason = "unkown reset";
      break;
  }
  return reason;
}
/*
 * Method to print the reason by which ESP32
 * has been awaken from sleep
 */
static const char *wakeup_reason_string(esp_deep_sleep_wakeup_cause_t wakeup_reason)
{
  const char *reason = "";

  switch(wakeup_reason)
  {
    case ESP_DEEP_SLEEP_WAKEUP_EXT0:
      reason = "Wakeup caused by external signal using RTC_IO";
      break;
    case ESP_DEEP_SLEEP_WAKEUP_EXT1:
      reason = "Wakeup caused by external signal using RTC_CNTL";
      break;
    case ESP_DEEP_SLEEP_WAKEUP_TIMER:
      reason = "Wakeup caused by timer";
      break;
    case ESP_DEEP_SLEEP_WAKEUP_TOUCHPAD:
      reason = "Wakeup caused by touchpad";
      break;
    case ESP_DEEP_SLEEP_WAKEUP_ULP:
      reason = "Wakeup caused by ULP program";
      break;
    default:
       reason = "Wakeup was not caused by deep sleep"; 
      break;
  }
  return reason;
}

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
  
  esp_deep_sleep_wakeup_cause_t wakeup_reason = esp_deep_sleep_get_wakeup_cause();
  if (wakeup_reason)
    dprintf("Boot: %s (bootCount: %d)", wakeup_reason_string(wakeup_reason), ++bootCount);
  dprintf("Boot: CPU(Pro): %s", reset_reason_string(rtc_get_reset_reason(0)));
  dprintf("Boot: CPU(App): %s", reset_reason_string(rtc_get_reset_reason(1)));
  dprintf("ESP32: Revision: %d (%d MHz)", ESP.getChipRevision(), ESP.getCpuFreqMHz());
  // dprintf("TIME: %s", __TIME__);
  // dprintf("DATE: %s", __DATE__);

#if 0
  esp_light_sleep_start();
#endif

#if 0
  // esp_err = gpio_pullup_dis(GPIO_NUM_xx);
  // esp_err = gpio_pulldown_en(GPIO_NUM_xx);
  int err = esp_deep_sleep_enable_ext0_wakeup((gpio_num_t)SW0,0); //1 = High, 0 = Low
  if (err) {
    dprintf("esp_deep_sleep_enable_ext0_wakeup: error %d", err);
    return;
  }
  esp_deep_sleep_enable_timer_wakeup(10000000); // or later esp_sleep_enable_timer_wakeup(10000000);
  dprintf("Enter deep sleep");
  esp_deep_sleep_start();
  // esp_light_sleep_start(); // does not exists?
#endif
}

#else
#error "Unkown platform"
#endif
#endif // FEATURE_LORA
#endif // ARDUINO 

