/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */
#ifdef ARDUINO 
#define RS_MAJOR    1
#define RS_MINOR    8

#ifdef ARDUINO_SAMD_ATMEL_SAMD21_XPRO_V1

#define FEATURE_LORA	1

#define	SW0				3		// switch needs pullup.
#define LED       LED_BUILTIN

#define MYSERIAL          Serial

#define LORA_SPI_MOSI   PIN_SPI_MOSI	// PA06 
#define LORA_SPI_MISO   PIN_SPI_MISO	// PA04
#define LORA_SPI_SCLK   PIN_SPI_SCK		// PA07
#define LORA_CS			    PIN_SPI_SS		// PA05
#define LORA_RESET      PIN_A0				// PB00
#define LORA_DIO0       PIN_A1	      // PB01 used for Rx, Tx Interrupt 	
#define LORA_DIO1       PIN_A2	  		// PA10 Fifo Level/Full, RxTimeout/Cad Detection Interrupt, unused in RadioShuttle
#define LORA_DIO2       PIN_A3	      // PA11 FhssChangeChannel when FreqHop is on (unused in RadioShuttle)
#define LORA_DIO3       PIN_A4	      // PA02 used Cad Detection in RS_Node_Offline/Checking mode
#define LORA_DIO4       PIN_A5        // PA03 FSK mode preamble detected, unused in RadioShuttle
#define LORA_DIO5       NC		        // NC   FSK mode ready / ClockOut, unused in RadioShuttle

#elif __SAMD21G18A__ // Zero
#define FEATURE_LORA  1

// #define BOARD_REV_200  1		// board with Lipo power supply/charger, mini USB
// #define BOARD_REV_301  1		// board with Lipo power supply/charger, micro USB
// #define BOARD_REV_630  1		// board with Lipo power supply/charger, micro USB
#define BOARD_REV_720  1		// Maker Faire Hannover revision, micro USB



#if defined(BOARD_REV_301) || defined(BOARD_REV_200)

#define SW0      		  1   // switch needs pullup.
#define LED       		0
#define MYSERIAL    	SerialUSB

#define LORA_SPI_MOSI   PIN_SPI_MOSI  // PA06?
#define LORA_SPI_MISO   PIN_SPI_MISO  // PA04?
#define LORA_SPI_SCLK   PIN_SPI_SCK   // PA07?
#define LORA_CS         2             // PA05?
#define LORA_RESET      38            // PB00?
#define LORA_DIO0       11            // used for Rx, Tx Interrupt
#define LORA_DIO1       13            // Fifo Level/Full, RxTimeout/Cad Detection Interrupt, unused in RadioShuttle
#define LORA_DIO2       10            // FhssChangeChannel when FreqHop is on, unused in RadioShuttle
#define LORA_DIO3       3             // used Cad Detection in RS_Node_Offline/Checking mode
#define LORA_DIO4       5             // FSK mode preamble detected, unused in RadioShuttle
#define LORA_DIO5       NC            // FSK mode ready / ClockOut, unused in RadioShuttle

#elif defined (BOARD_REV_630) || defined (BOARD_REV_720)

#define SW0       		12              // PA19 switch needs pullup
#define LED       		LED_BUILTIN     // PA17
#define MYSERIAL    	SerialUSB

#define LORA_SPI_MOSI   PIN_SPI_MOSI  // PB10
#define LORA_SPI_MISO   PIN_SPI_MISO  // PA12
#define LORA_SPI_SCLK   PIN_SPI_SCK   // PB11
#define LORA_CS         3             // PA09
#define LORA_RESET      38            // PA13
#define LORA_DIO0       16            // PB09 used for Rx, Tx Interrupt
#define LORA_DIO1       5             // PA15 Fifo Level/Full, RxTimeout/Cad Detection Interrupt, unused in RadioShuttle
#define LORA_DIO2       2             // PA14 FhssChangeChannel when FreqHop is on, unused in RadioShuttle
#define LORA_DIO3       18            // PA05 used Cad Detection in RS_Node_Offline/Checking mode
#define LORA_DIO4       17            // PA04 FSK mode preamble detected, unused in RadioShuttle
#define LORA_DIO5       NC            //      FSK mode ready / ClockOut, unused in RadioShuttle

#define BOOSTER_EN33    9             // Enable 3.3 volt 150mA max
#define BOOSTER_EN50    8             // Enable 5.0 volt 150mA max
#define DISPLAY_EN      4             // Turn on display power (3.3 V must be enabled first)


#elif defined(ARDUINO_SAMD_FEATHER_M0) // Feather M0 w/Radio

#define SW0           12              // switch needs pullup, must be conected to the headers
#define LED           LED_BUILTIN     // 13
#define MYSERIAL      Serial          // this is a USB Serial, however the Feather M0 calls it only Serial.

#define LORA_SPI_MOSI   PIN_SPI_MOSI  // PA12
#define LORA_SPI_MISO   PIN_SPI_MISO  // PB10
#define LORA_SPI_SCLK   PIN_SPI_SCK   // PB11
#define LORA_CS         8             // PA06
#define LORA_RESET      4             // PA08
#define LORA_DIO0       3             // PA09 used for Rx, Tx Interrupt, Cad Detection
#define LORA_DIO1       NC            // Fifo Level/Full, RxTimeout/Cad Detection Interrupt, unused in RadioShuttle
#define LORA_DIO2       NC            // FhssChangeChannel when FreqHop is on, unused in RadioShuttle
#define LORA_DIO3       NC            // used Cad Detection in RS_Node_Offline/Checking mode
#define LORA_DIO4       NC            // FSK mode preamble detected, unused in RadioShuttle
#define LORA_DIO5       NC            // FSK mode ready / ClockOut, unused in RadioShuttle


/*
 * PIN_LED_RXL and PIN_LED_TXL are already defined.
 */
#else

#error "Unkown D21 board revision"

#endif

#elif defined(ARDUINO_ARCH_ESP32)   // the Heltec Board
#define FEATURE_LORA  1

#define SW0           0               // no pullup, TODO check setup code
#define LED           2               // 
#define MYSERIAL      Serial          // this is a USB Serial, however the Feather M0 calls it only Serial.

#define LORA_SPI_MOSI   MOSI          // MOSI 27 Heltec, 23 Arduino-Dev
#define LORA_SPI_MISO   MISO          // MISO 19 Heltec, 19 Arduino-Dev
#define LORA_SPI_SCLK   SCK           // SCK   5 Heltec, 18 Arduino-Dev
#define LORA_CS         18            // LORA_DEFAULT_SS_PIN 
#define LORA_RESET      14            // LORA_DEFAULT_RESET_PIN
#define LORA_DIO0       26            // LORA_DEFAULT_DIO0_PIN
#define LORA_DIO1       NC            // Fifo Level/Full, RxTimeout/Cad Detection Interrupt, unused in RadioShuttle
#define LORA_DIO2       NC            // FhssChangeChannel when FreqHop is on, unused in RadioShuttle
#define LORA_DIO3       NC            // used Cad Detection in RS_Node_Offline/Checking mode
#define LORA_DIO4       NC            // FSK mode preamble detected, unused in RadioShuttle
#define LORA_DIO5       NC            // FSK mode ready / ClockOut, unused in RadioShuttle

#ifdef ARDUINO_Heltec_WIFI_LoRa_32
#undef LED
#define LED             25
#define DISPLAY_ADDRESS 0x3c
#define DISPLAY_SDA     4
#define DISPLAY_SCL     15
#define DISPLAY_RESET   16
#endif

#else 
#error "unkown board"
#endif

#endif // Arduino

