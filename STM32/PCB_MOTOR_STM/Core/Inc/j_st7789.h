/*
 * start.h
 *
 *  Created on: Feb 4, 2026
 *      Author: jonar
 */

#ifndef INC_START_H_
#define INC_START_H_
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"


void start_code();

void write_cmd(uint8_t CMD);
void write_u8(uint8_t* dat, size_t len);
void write_s8(uint8_t dat);
void init_lcd();
void fill_area(uint16_t color);

#endif /* INC_START_H_ */
