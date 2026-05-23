#pragma once
#include <Arduino.h>

#ifndef PANEL_W
#define PANEL_W 64
#endif
#ifndef PANEL_H
#define PANEL_H 64
#endif

extern uint16_t strawberry[PANEL_H][PANEL_W];
void buildStrawberry();
