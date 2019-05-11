/*
 nuts_bolts.c - Shared functions
 Part of Grbl

 Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
 Copyright (c) 2009-2011 Simen Svale Skogsrud

 */

#include "grbl.h"

#define MAX_INT_DIGITS 8 // Maximum number of digits in int32 (and float)

// Extracts a floating point value from a string. The following code is based loosely on
// the avr-libc strtod() function by Michael Stumpf and Dmitry Xmelkov and many freely
// available conversion method examples, but has been highly optimized for Grbl. For known
// CNC applications, the typical decimal value is expected to be in the range of E0 to E-4.
// Scientific notation is officially not supported by g-code, and the 'E' character may
// be a g-code word on some CNC systems. So, 'E' notation will not be recognized.
// NOTE: Thanks to Radu-Eosif Mihailescu for identifying the issues with using strtod().
uint8_t read_float(char *line, uint8_t *char_counter, float *float_ptr)
{
	char *ptr = line + *char_counter;
	unsigned char c;

	// Grab first character and increment pointer. No spaces assumed in line.
	c = *ptr++;

	// Capture initial positive/minus character
	bool isnegative = false;
	if (c == '-')
	{
		isnegative = true;
		c = *ptr++;
	}
	else if (c == '+')
	{
		c = *ptr++;
	}

	// Extract number into fast integer. Track decimal in terms of exponent value.
	uint32_t intval = 0;
	int8_t exp = 0;
	uint8_t ndigit = 0;
	bool isdecimal = false;
	while (1)
	{
		c -= '0';
		if (c <= 9)
		{
			ndigit++;
			if (ndigit <= MAX_INT_DIGITS)
			{
				if (isdecimal)
				{
					exp--;
				}
				intval = (((intval << 2) + intval) << 1) + c; // intval*10 + c
			}
			else
			{
				if (!(isdecimal))
				{
					exp++;  // Drop overflow digits
				}
			}
		}
		else if (c == (('.' - '0') & 0xff) && !(isdecimal))
		{
			isdecimal = true;
		}
		else
		{
			break;
		}
		c = *ptr++;
	}

	// Return if no digits have been read.
	if (!ndigit)
	{
		return (false);
	};

	// Convert integer into floating point.
	float fval;
	fval = (float) intval;

	// Apply decimal. Should perform no more than two floating point multiplications for the
	// expected range of E0 to E-4.
	if (fval != 0)
	{
		while (exp <= -2)
		{
			fval *= 0.01;
			exp += 2;
		}
		if (exp < 0)
		{
			fval *= 0.1;
		}
		else if (exp > 0)
		{
			do
			{
				fval *= 10.0;
			} while (--exp > 0);
		}
	}

	// Assign floating point value with correct sign.
	if (isnegative)
	{
		*float_ptr = -fval;
	}
	else
	{
		*float_ptr = fval;
	}

	*char_counter = ptr - line - 1; // Set char_counter to next statement

	return (true);
}

void delay_ms(uint16_t ms)
{
	//delay(ms);
	ets_delay_us(ms * 1000);
}

// Non-blocking delay function used for general operation and suspend features.
void delay_sec(float seconds, uint8_t mode)
{
	uint16_t i = ceil(1000 / DWELL_TIME_STEP * seconds);
	while (i-- > 0)
	{
		if (sys.abort)
		{
			return;
		}
		if (mode == DELAY_MODE_DWELL)
		{
			protocol_execute_realtime();
		}
        else     // DELAY_MODE_SYS_SUSPEND
        {
			// Execute rt_system() only to avoid nesting suspend loops.
			protocol_exec_rt_system();
		}
		//delay(DWELL_TIME_STEP); // Delay DWELL_TIME_STEP increment
		ets_delay_us(DWELL_TIME_STEP * 1000);
	}
}

// Simple hypotenuse computation function.
float hypot_f(float x, float y)
{
	return (sqrt(x * x + y * y));
}

float convert_delta_vector_to_unit_vector(float *vector)
{
	uint8_t idx;
	float magnitude = 0.0;
	for (idx = 0; idx < N_AXIS; idx++)
	{
		if (vector[idx] != 0.0)
		{
			magnitude += vector[idx] * vector[idx];
		}
	}
	magnitude = sqrt(magnitude);
	float inv_magnitude = 1.0 / magnitude;
	for (idx = 0; idx < N_AXIS; idx++)
	{
		vector[idx] *= inv_magnitude;
	}
	return (magnitude);
}

float limit_value_by_axis_maximum(float *max_value, float *unit_vec)
{
	uint8_t idx;
	float limit_value = SOME_LARGE_VALUE;
	for (idx = 0; idx < N_AXIS; idx++)
	{
        if (unit_vec[idx] != 0)    // Avoid divide by zero.
        {
			limit_value = MIN(limit_value, fabs(max_value[idx] / unit_vec[idx]));
		}
	}
	return (limit_value);
}

// int constrain(int val, int min, int max) {
// return (val < min)? min : (val > max)? max : val;
// }

// long map(long x, long in_min, long in_max, long out_min, long out_max)
// {
// return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
// }

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

long mapConstrain(long x, long in_min, long in_max, long out_min, long out_max)
{
	x = constrain(x, out_min, out_max);
	return map(x, in_min, in_max, out_min, out_max);
}
