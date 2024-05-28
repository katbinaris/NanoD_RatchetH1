/*
Oncetop Motor
Primary Recommended Motor 
*/
#pragma once

#include "nanofoc_d.h"

#if ONCETOP

static const int blcd_pp = 7; // Pole Pairs
static const float bldc_pr = 5.3; // Terminal Resistance
static const int bldc_KV = 370; // KV Rating
static const float bldc_Li = 0.016; // Terminal Inductance
static const float driver_supply = 4.75;
static const float driver_voltage_limit = 4.5;

/* 
MAD Motor 
Alternative High Torque Motor
*/
#elif MADMOTOR

static const int blcd_pp = 7; // Pole Pairs
static const float bldc_pr = 0; // Terminal Resistance
static const int bldc_KV = 0; // KV Rating
static const float bldc_Li = 0; // Terminal Inductance (mH)
static const float driver_supply = 4.75;
static const float driver_voltage_limit = 4.5;

#endif