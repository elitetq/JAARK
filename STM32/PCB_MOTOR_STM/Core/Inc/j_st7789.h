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

typedef enum {
    J_WHITE = 0xFFFF,
    J_BLACK = 0x0000,
    J_BLUE = 0x001F,
    J_CYAN = 0x00EF,
    J_RED = 0xF000,
    J_PINK = 0xF00F,
} j_color;

typedef enum {
    J_TEXT = 0,
    J_SHAPE
} j_entity_type;

typedef struct {
    j_entity_type type;
    uint8_t x, y;
    
} j_entity;

typedef struct {
    j_color bgcol;
    uint8_t num_entities;
    
} j_struct;


void write_cmd(uint8_t CMD);
void write_u8(uint8_t* dat, size_t len);
void write_s8(uint8_t dat);
void init_lcd();
void fill_area(uint16_t color);

#endif /* INC_START_H_ */
