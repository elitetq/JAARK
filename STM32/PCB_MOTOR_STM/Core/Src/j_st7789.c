/*
 * start.c
 *
 *  Created on: Feb 4, 2026
 *      Author: jonar
 */
#include "j_st7789.h"
static uint8_t col_arr[J_BUF_SIZE];

void start_code() {
}

void write_cmd(uint8_t CMD) {
    HAL_GPIO_WritePin(J_DC_PORT,J_DC,0);

    HAL_SPI_Transmit(&hspi3,&CMD,1,100);

}
void write_u8(uint8_t* dat, size_t len) {
    HAL_GPIO_WritePin(J_DC_PORT,J_DC,1);
    HAL_SPI_Transmit(&hspi3,dat,len,100);
    
}
void write_s8(uint8_t dat) {
    HAL_GPIO_WritePin(J_DC_PORT,J_DC,1);
    HAL_SPI_Transmit(&hspi3,&dat,1,100);
    
}

void init_lcd() {
	HAL_GPIO_WritePin(J_RES_PORT,J_RES,0);
    HAL_Delay(50);
    HAL_GPIO_WritePin(J_RES_PORT,J_RES,1);
    HAL_Delay(0);
    write_cmd(CMD_SWFRES);
    HAL_Delay(150);

    write_cmd(CMD_SLPOUT);
    HAL_Delay(120);
    write_cmd(CMD_COLMOD);
    write_s8(0x55);

    write_cmd(CMD_NORON);
    write_cmd(CMD_DISPON);
    write_cmd(CMD_INVON);
    uint8_t bounds[] = {0,0,0,0xEF};
    write_cmd(CMD_CASET);
    write_u8(bounds,4);
    write_cmd(CMD_RASET);
    write_u8(bounds,4);
    write_cmd(CMD_RAMWR);
}

void fill_area(uint16_t color) {
    uint8_t lval = color;
    uint8_t hval = color >> 8;
    for(int i = 0; i < J_BUF_SIZE; i+= 2) {
        col_arr[i] = hval;
        col_arr[i+1] = lval;
    }
    for(int i = 0; i < J_DIV; i++) {
        write_u8(col_arr,J_BUF_SIZE);
    }
}

