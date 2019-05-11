/*
 print.h - Functions for formatting output strings
 Part of Grbl

 Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
 Copyright (c) 2009-2011 Simen Svale Skogsrud

 */

#ifndef print_h
#define print_h

void printString(const char *s);

void printPgmString(const char *s);

void printInteger(long n);

void print_uint32_base10(uint32_t n);

// Prints an uint8 variable in base 10.
void print_uint8_base10(uint8_t n);

// Prints an uint8 variable in base 2 with desired number of desired digits.
void print_uint8_base2_ndigit(uint8_t n, uint8_t digits);

void printFloat(float n, uint8_t decimal_places);

// Floating value printing handlers for special variables types used in Grbl.
//  - CoordValue: Handles all position or coordinate values in  mm reporting.
//  - RateValue: Handles feed rate and current velocity in  mm reporting.
void printFloat_CoordValue(float n);
void printFloat_RateValue(float n);

#endif
