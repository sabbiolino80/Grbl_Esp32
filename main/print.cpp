/*
 print.c - Functions for formatting output strings
 Part of Grbl

 Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
 Copyright (c) 2009-2011 Simen Svale Skogsrud

 */

#include "grbl.h"

// void printIntegerInBase(unsigned long n, unsigned long base)
// {
// 	unsigned char buf[8 * sizeof(long)]; // Assumes 8-bit chars.
// 	unsigned long i = 0;
//
// 	if (n == 0) {
// 		serial_write('0');
// 		return;
// 	}
//
// 	while (n > 0) {
// 		buf[i++] = n % base;
// 		n /= base;
// 	}
//
// 	for (; i > 0; i--)
// 		serial_write(buf[i - 1] < 10 ?
// 			'0' + buf[i - 1] :
// 			'A' + buf[i - 1] - 10);
// }

// Prints an uint8 variable in base 10.
void print_uint8_base10(uint8_t n)
{
	uint8_t digit_a = 0;
	uint8_t digit_b = 0;
    if (n >= 100)   // 100-255
    {
		digit_a = '0' + n % 10;
		n /= 10;
	}
    if (n >= 10)   // 10-99
    {
		digit_b = '0' + n % 10;
		n /= 10;
	}
	serial_write('0' + n);
	if (digit_b)
	{
		serial_write(digit_b);
	}
	if (digit_a)
	{
		serial_write(digit_a);
	}
}

// Prints an uint8 variable in base 2 with desired number of desired digits.
void print_uint8_base2_ndigit(uint8_t n, uint8_t digits)
{
	unsigned char buf[digits];
	uint8_t i = 0;

	for (; i < digits; i++)
	{
		buf[i] = n % 2;
		n /= 2;
	}

	for (; i > 0; i--)
		Serial.print('0' + buf[i - 1]);
}

void print_uint32_base10(uint32_t n)
{
	if (n == 0)
	{
		Serial.print('0');
		return;
	}

	unsigned char buf[10];
	uint8_t i = 0;

	while (n > 0)
	{
		buf[i++] = n % 10;
		n /= 10;
	}

	for (; i > 0; i--)
		Serial.print('0' + buf[i - 1]);
}

void printInteger(long n)
{
	if (n < 0)
	{
		Serial.print('-');
		print_uint32_base10(-n);
	}
	else
	{
		print_uint32_base10(n);
	}
}

// Convert float to string by immediately converting to a long integer, which contains
// more digits than a float. Number of decimal places, which are tracked by a counter,
// may be set by the user. The integer is then efficiently converted to a string.
// NOTE: AVR '%' and '/' integer operations are very efficient. Bitshifting speed-up
// techniques are actually just slightly slower. Found this out the hard way.
void printFloat(float n, uint8_t decimal_places)
{
	Serial.print(n, decimal_places);
}

// Floating value printing handlers for special variables types used in Grbl and are defined
// in the config.h.
//  - CoordValue: Handles all position or coordinate values in  mm reporting.
//  - RateValue: Handles feed rate and current velocity in  mm reporting.
void printFloat_CoordValue(float n)
{
	printFloat(n, N_DECIMAL_COORDVALUE_MM);
}

void printFloat_RateValue(float n)
{
	printFloat(n, N_DECIMAL_RATEVALUE_MM);
}
