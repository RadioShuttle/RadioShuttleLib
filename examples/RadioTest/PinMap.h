/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifdef _VARIANT_ATMEL_SAMD21_XPRO_
#define FEATURE_LORA	1

#define	SW0				3		// switch needs pullup.
#define LED             LED_BUILTIN

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

// #define BOARD_REV_200  1		// board with Lipo power supply/charger, mini USB
// #define BOARD_REV_301  1		// board with Lipo power supply/charger, micro USB
#define BOARD_REV_630     1		// board with Lipo power supply/charger, micro USB
// #define BOARD_REV_720  1		// Maker Faire Hannoer revision, micro USB

#define FEATURE_LORA  1


#ifdef BOARD_REV_301

#define SW0      		1   // switch needs pullup.
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

#elif BOARD_REV_630

#define SW0       		12              // switch needs pullup.
#define LED       		LED_BUILTIN     // 13
#define MYSERIAL    	SerialUSB

#define LORA_SPI_MOSI   PIN_SPI_MOSI  // PA06?
#define LORA_SPI_MISO   PIN_SPI_MISO  // PA04?
#define LORA_SPI_SCLK   PIN_SPI_SCK   // PA07?
#define LORA_CS         3             // PA05?
#define LORA_RESET      38            // PB00?
#define LORA_DIO0       16            // used for Rx, Tx Interrupt
#define LORA_DIO1       5             // Fifo Level/Full, RxTimeout/Cad Detection Interrupt, unused in RadioShuttle
#define LORA_DIO2       2             // FhssChangeChannel when FreqHop is on, unused in RadioShuttle
#define LORA_DIO3       18            // PA05 used Cad Detection in RS_Node_Offline/Checking mode
#define LORA_DIO4       17            // FSK mode preamble detected, unused in RadioShuttle
#define LORA_DIO5       NC            // FSK mode ready / ClockOut, unused in RadioShuttle

#define BOOSTER_EN33    9
#define BOOSTER_EN50    8

/*
 * PIN_LED_RXL and PIN_LED_TXL are already defined.
 */

#else
#error "Unkown Board revision"
#endif

#else
#error "unkown board"
#endif
