/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2018 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifdef ARDUINO 

#include "xPinMap.h"
#include <arduino-mbed.h>
#include <arduino-util.h>
#include <NVProperty_Editor.h>
#if defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_ARCH_SAMD)
#include <RTCZero.h>
#elif ARDUINO_ARCH_ESP32
#include <rom/rtc.h>
#include <driver/adc.h>
#include <NTPUpdate.h>
#if defined (FEATURE_SI7021) || defined (FEATURE_RTC_DS3231)
 #include <Wire.h>
#endif
#ifdef FEATURE_RTC_DS3231
 #include "ds3231.h"
#endif
#ifdef FEATURE_SI7021
  #include "HELIOS_Si7021.h"
#endif
#endif
#if LORA_CS != NC
#include <sx1276-mbed-hal.h>
#endif

#ifdef FEATURE_LORA

NVProperty prop; // global property store supports OTP, Flash and SRAM
static void InitPropertyEditor(InterruptIn *intrButton);


void InitLoRaChipWithShutdown()
{
#ifdef LORA_CS
    if (LORA_CS == NC)
      return;
    Radio *radio = new SX1276Generic(NULL, RFM95_SX1276,
                            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);
    RadioEvents_t radioEvents;
    memset(&radioEvents, 0, sizeof(radioEvents));
    if (radio->Init(&radioEvents)) {
        radio->Sleep();
        delete radio;
    }
#endif
}

bool ESP32DeepsleepWakeup;

#if defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_ARCH_SAMD)

#ifdef BAT_MESURE_ADC
float GetBatteryVoltage(bool print)
{
#ifdef D21_LONGRA_REV_750
  if (DSU->STATUSB.bit.DBGPRES) // skip this when the debugger uses the SWD pin
    return 0;
#endif
  int adcBits = 12;
  int adcSampleCount = 12;
  float volt = 0;
  float vref = 1.0;

#ifdef BAT_MESURE_EN
  DigitalOut swd(BAT_MESURE_EN);
  swd = 0;
#endif

  analogReadResolution(adcBits);
  analogReference(AR_INTERNAL1V0);

  float adcValue = 0;
  for (int i = 0; i < adcSampleCount; i++) {
    adcValue += analogRead(BAT_MESURE_ADC);
  }
  adcValue /= (float)adcSampleCount;
  adcValue += 0.5; // for proper rounding
  
  float voltstep = vref/(float)(1<<adcBits); // e.g. 12 bit 4096
  volt = (float) (adcValue * voltstep) * BAT_VOLTAGE_DIVIDER; // 82k, 82k+220k

  if (print)
    dprintf("Power: %s (ADC: %d Vref: %s)", String(volt).c_str(), (int)adcValue, String(vref, 3).c_str()); 

#ifdef BAT_MESURE_EN
  DigitalIn swdin2(BAT_MESURE_EN);
#endif
  DigitalIn adcin(BAT_MESURE_ADC);
  return volt;
}
#endif

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

void RTCInit(const char *date, const char *timestr, InterruptIn *intrButton)
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
#ifdef BAT_MESURE_ADC
  GetBatteryVoltage();
#endif
  /*
   * the default is 32 secs, at present it cannot be changed.
   */
  InitWatchDog();
  InitPropertyEditor(intrButton);
}

bool runPropertyEdtior()
{
	NVPropertyEditorInit(&MYSERIAL);
	NVPropertyEditor();
	NVIC_SystemReset();
	return false;
}

#elif ARDUINO_ARCH_ESP32 

uint64_t ESP32WakeupGPIOStatus;

#ifdef FEATURE_SI7021
HELIOS_Si7021 *sensorSI7021;  
#endif

#if defined(ESP32_ECO_POWER_REV_1) || defined(ARDUINO_HELTEC_WIFI_LORA_32_V2) || defined(ARDUINO_heltec_wifi_lora_32_V2) || defined(ARDUINO_HELTEC_WIRELESS_STICK)
float GetBatteryVoltage(bool print)
{
  int adcBits = 12;
  int adcSampleCount = 12;
  float volt = 0;
  float correct = 0;
  float vref;
#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2) || defined(ARDUINO_heltec_wifi_lora_32_V2)
  vref = (float)prop.GetProperty(prop.ADC_VREF, 1100)/1000.0;
  vref = (vref / 1.100) * 1.995;
  adc_attenuation_t adc_attn = ADC_6db;
  correct = -0.18;
#else
  vref = (float)prop.GetProperty(prop.ADC_VREF, 1100)/1000.0;
  adc_attenuation_t adc_attn = ADC_0db;
  correct = -0.22;
#endif


#ifdef BAT_MESURE_EN
  DigitalOut extPWR(BAT_MESURE_EN);
  extPWR = EXT_POWER_ON;
#endif
  
  analogReadResolution(adcBits);
  analogSetAttenuation(adc_attn);
  analogSetPinAttenuation(BAT_MESURE_ADC, adc_attn); 

  float adcValue = 0;
  for (int i = 0; i < adcSampleCount; i++) {
    adcValue += analogRead(BAT_MESURE_ADC); // BAT_MESURE_ADC is ADC1
  }

#ifdef BAT_MESURE_EN
  extPWR = EXT_POWER_OFF; 
  DigitalIn tmpPWR(BAT_MESURE_EN);
  tmpPWR.mode(PullUp); // turn off the power, PullUp will keep it off in deepsleep
#endif  
  
  adcValue /= (float)adcSampleCount;
  adcValue += 0.5; // for proper rounding.

  float voltstep = vref/(float)(1<<adcBits); // e.g. 12 bit 4096
  volt = (float) (adcValue * voltstep) * BAT_VOLTAGE_DIVIDER; // 82k, 82k+220k
  volt += correct; // correction in millivolts
  
  if (print)
    dprintf("Power: %.2fV (ADC: %d Vref: %.3f)", volt, (int)adcValue, vref); 
  return volt;
}

#endif

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool hasSensor;

void RTCUpdateHandler(void);

void RTCInit(const char *date, const char *timestr, InterruptIn *intrButton)
{
#if defined (FEATURE_SI7021) || defined (FEATURE_RTC_DS3231)
  Wire.begin();
#endif
#if defined (ARDUINO_HELTEC_WIFI_LORA_32_V2) || defined(ARDUINO_heltec_wifi_lora_32_V2) || defined(ARDUINO_ESP32C3_DEV)
  delay(100); // needs time to flush bootloader garbage
#endif
  NTPUpdate ntp;

  time_t now = time(NULL);
  
#ifdef FEATURE_RTC_DS3231
  DS3231_init(DS3231_CONTROL_INTCN);
  struct ts ds;
  DS3231_get(&ds);
  if (!now || now < ds.unixtime || now > ds.unixtime) {
    time_t t = cvt_date(date, timestr);
    t -= (ntp.GetGMTOffset() +  ntp.GetDayLightOffset(t)); // convert to UTC

    if (!ds.unixtime || t > ds.unixtime ) {
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
    tv.tv_sec = ds.unixtime;
    struct timezone tz;
    memset(&tz, 0, sizeof(tz));
    tz.tz_minuteswest = (ntp.GetGMTOffset() +  ntp.GetDayLightOffset(t)/60);
    tz.tz_dsttime = 1;
    ntp.SetTimeZone(tv.tv_sec);
    settimeofday(&tv, &tz);
    dprintf("RTC Clock: %d/%d/%d %02d:%02d:%02d UTC", ds.mday, ds.mon, ds.year, ds.hour, ds.min, ds.sec);
  }
  int i = prop.GetProperty(prop.RTC_AGING_CAL, 0);
  if (i && i != DS3231_get_aging())
    DS3231_set_aging(i);
  ntp.SetRTCUpdateProc(&RTCUpdateHandler);
#else // without ds3231
  if (!now) {
    time_t t = cvt_date(date, timestr);
    t -= (ntp.GetGMTOffset() +  ntp.GetDayLightOffset(t)); // convert to UTC
    struct timeval tv;
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = t;
    struct timezone tz;
    memset(&tz, 0, sizeof(tz));
    tz.tz_minuteswest = (ntp.GetGMTOffset() +  ntp.GetDayLightOffset(t))/60;
    tz.tz_dsttime = 1;
    ntp.SetTimeZone(tv.tv_sec);
    settimeofday(&tv, &tz);
  }
#endif

  ESP32WakeupGPIOStatus = ESP32WakeupGPIOStatusLow | (uint64_t) ESP32WakeupGPIOStatusHigh << 32;
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason)
    dprintf("Boot: %s (bootCount: %d)", ESP32WakeUpReason(wakeup_reason), ++bootCount);
  dprintf("Boot: CPU(Pro): %s", ESP32ResetReason(rtc_get_reset_reason(0)));
  if (ESP.getChipCores() == 2)
    dprintf("Boot: CPU(App): %s", ESP32ResetReason(rtc_get_reset_reason(1)));
  dprintf("%s: Rev: %d, %d MHz, IDF(%s)", ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(),  ESP.getSdkVersion());


  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) { 
    /*
     * Run this only on reset boot, not from deepsleep.
     */
#ifdef EXT_POWER_SW
    DigitalIn extPWR(EXT_POWER_SW);
    extPWR.mode(PullUp); // turn off the power, PullUp will keep it off in deepsleep
#endif
#ifdef BAT_MESURE_ADC
    GetBatteryVoltage(); 
#endif

#ifdef FEATURE_SI7021
  sensorSI7021 = new HELIOS_Si7021();
  if (sensorSI7021->hasSensor()) {
      hasSensor = true;
      dprintf("%s: Rev(%d)  %.2f°C  Humidity: %.2f%%", sensorSI7021->getModelName(), sensorSI7021->getRevision(), sensorSI7021->readTemperature(), sensorSI7021->readHumidity());
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
       sensorSI7021 = new HELIOS_Si7021();
       dprintf("%s: %.2f°C  Humidity: %.2f%%", sensorSI7021->getModelName(), sensorSI7021->readTemperature(), sensorSI7021->readHumidity());
     }
#endif
  }
  /*
   * the default of two minutes should be fine for networking hangs,  etc.
   * specify optional parameter in ms e.g.:(120 * 1000) 
   */
  InitWatchDog();
  InitPropertyEditor(intrButton);
}

void
RTCUpdateHandler(void)
{
  /*
   * will be called when the RTCUpdate issues an update.
   */
#ifdef FEATURE_RTC_DS3231
  time_t t = time(NULL);
  struct ts ds;
  struct tm *tmp = gmtime(&t);
  ds.sec = tmp->tm_sec;
  ds.min = tmp->tm_min;
  ds.hour = tmp->tm_hour;
  ds.wday = tmp->tm_wday;
  ds.mday = tmp->tm_mday;
  ds.mon = tmp->tm_mon + 1;
  ds.year =  tmp->tm_year + 1900;
  DS3231_set(ds);
  dprintf("RTC update issued");
#endif
}

bool runPropertyEdtior()
{
	NVPropertyEditorInit(&MYSERIAL);
	NVPropertyEditor();
	esp_restart();
	return false;
}

#else
#error "Unkown platform"
#endif


/*
 * The automatic property editor startup:
 * When the RTCInit is being called with a button parameter
 * the InitPropertyEditor installs a Timer called every second.
 * The userButtonTimerFunc will check if the button is hold down
 * for 5 continues seconds and installs a callback to start
 * the editor later via sleep/deepsleep code within the loop.
 * We cannot call the Editor within the Interrupt function,
 * therefore the sleep/deepsleep processing calls the
 * runPropertyEdtior function via the registered callback.
 */
RTC_DATA_ATTR int buttonCount = 0;
RTC_DATA_ATTR int timerCount = 0;

Timeout *userButtonTimer;
InterruptIn *userButtonIntr;

static void userButtonTimerFunc(void)
{
	if (timerCount++ >= 10) {
		userButtonTimer->detach();
		return;
	}
	userButtonTimer->attach(callback(&userButtonTimerFunc), 1000);
	if (userButtonIntr->read() == 0)
		buttonCount++;
	else
		buttonCount = 0;

	if (buttonCount >= 5) {
		userButtonTimer->detach();
		idleCbs.RegisterIdeCallback(callback(&runPropertyEdtior));
	}
}

static void InitPropertyEditor(InterruptIn *intrButton)
{
	if (!intrButton)
		return;
	userButtonIntr = (InterruptIn *)intrButton;
	if (!userButtonTimer)
		userButtonTimer = new Timeout;
	userButtonTimerFunc(); //  start Timer
}

#endif // FEATURE_LORA
#endif // ARDUINO 
