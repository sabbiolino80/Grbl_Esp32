/*
    eeprom.h - Header for system level commands and real-time processes
    Part of Grbl
    Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC

*/

#ifndef eeprom_h
#define eeprom_h

#include "grbl.h"

void memcpy_to_eeprom_with_checksum(unsigned int destination, char *source, unsigned int size);
int memcpy_from_eeprom_with_checksum(char *destination, unsigned int source, unsigned int size);

#endif
