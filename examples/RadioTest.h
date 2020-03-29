/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifndef __RADIOTEST_H__
#define __RADIOTEST_H__

extern int InitRadio(void);
extern void DeInitRadio(void);
extern void RadioTest();
extern int RadioUpdate(bool keyPressed);
extern bool RadioISIdle();
extern void InitLoRaChipWithShutdown();
extern void RadioContinuesTX(void);

#endif // RadioTest.h
