/*
 * start.c
 *
 *  Created on: Feb 4, 2026
 *      Author: jonar
 */
#include "j_st7789.h"
static uint8_t col_arr[J_BUF_SIZE];


void set_bounds(uint8_t xlo, uint8_t xhi, uint8_t ylo, uint8_t yhi) {
    if(j_flag_set_bounds) {
        j_bounds[0] = xlo;
        j_bounds[1] = xhi;
        j_bounds[2] = ylo;
        j_bounds[3] = yhi;
    }
    uint8_t bounds_x[] = {0,xlo,0,xhi};
    uint8_t bounds_y[] = {0,ylo,0,yhi};
    write_cmd(CMD_CASET);
    write_u8(bounds_x,4);
    write_cmd(CMD_RASET);
    write_u8(bounds_y,4);
    write_cmd(CMD_RAMWR);
}

void write_cmd(uint8_t CMD) {
    HAL_GPIO_WritePin(J_CFG_DC_PORT,J_CFG_DC_PIN,0);

    HAL_SPI_Transmit(&J_CFG_SPI_OBJ,&CMD,1,100);

}
void write_u8(uint8_t* dat, size_t len) {
    HAL_GPIO_WritePin(J_CFG_DC_PORT,J_CFG_DC_PIN,1);
    HAL_SPI_Transmit(&J_CFG_SPI_OBJ,dat,len,100);
    
}
void write_s8(uint8_t dat) {
    HAL_GPIO_WritePin(J_CFG_DC_PORT,J_CFG_DC_PIN,1);
    HAL_SPI_Transmit(&J_CFG_SPI_OBJ,&dat,1,100);
    
}

void init_lcd() {
	HAL_GPIO_WritePin(J_CFG_RES_PORT,J_CFG_RES_PIN,0);
    HAL_Delay(50);
    HAL_GPIO_WritePin(J_CFG_RES_PORT,J_CFG_RES_PIN,1);
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

/**
 * void fill_text(uint8_t x, uint8_t y, char* str, uint8_t font_size, j_color color) {
    j_flag_set_bounds = 0;
    int i = 0;
    while(str[i] != 0 && i < J_MAX_PRINT_SIZE) {
        set_bounds(x,x+j_fonts_length[font_size]-1,y,y+j_fonts_height[font_size]-1);
        draw_text(str[i],font_size,color);
        x+= (j_fonts_length[font_size] + 5);
        i++;

    }
    set_bounds(j_bounds[0],j_bounds[1],j_bounds[2],j_bounds[3]);
    j_flag_set_bounds = 1;
}

void draw_text(char ch, uint8_t font_size, j_color FILL_COL) {
    j_color BG_COL = j_master_control.bgcol;
    uint16_t* char_ptr = char_ptr_ret(ch,font_size);


    for(int i = 0; i < j_fonts_height[font_size]; i++) {
        for(int j = 0; j < j_fonts_length[font_size]*2; j+= 2) {
            j_color ret_color = ((char_ptr[i] << (j/2)) & 0x8000) == 0x8000 ? FILL_COL : BG_COL;
            j_fonts_buffer[j] = (uint8_t)(ret_color >> 8);
            j_fonts_buffer[j+1] = (uint8_t)(ret_color);
        }
        write_u8(j_fonts_buffer,j_fonts_length[font_size]*2);
    }
}
 */
