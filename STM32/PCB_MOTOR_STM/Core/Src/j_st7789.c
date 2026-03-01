/*

 *
 *  Created on: Feb 4, 2026
 *      Author: jonar
 */
#include "j_st7789.h"
static uint8_t col_arr[J_BUF_SIZE];
static int ten_power = pow(10,MAX_DECIMAL_SIZE);
static char float_to_str_buf[MAX_TEXT_TO_STRING_SIZE];


void set_bounds(uint8_t xlo, uint8_t xhi, uint8_t ylo, uint8_t yhi) {
    uint8_t temp;
    if(xlo > xhi) {
        temp = xhi;
        xhi = xlo;
        xlo = temp;
    }
    if(ylo > yhi) {
        temp = yhi;
        yhi = ylo;
        ylo = temp;
    }
    if(j_flag_set_bounds) {
        j_bounds[0] = xlo;
        j_bounds[1] = xhi-1;
        j_bounds[2] = ylo;
        j_bounds[3] = yhi-1;
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
    //HAL_Delay(150);

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
    j_master_control.bgcol = color;
    for(int i = 0; i < J_BUF_SIZE; i+= 2) {
        col_arr[i] = hval;
        col_arr[i+1] = lval;
    }
    for(int i = 0; i < J_DIV; i++) {
        write_u8(col_arr,J_BUF_SIZE);
    }
}


void fill_text(uint8_t x, uint8_t y, char* str, int len, uint8_t font_size, j_color color, j_centering CENTERING) {
    j_flag_set_bounds = 0;
    int i = 0;
    uint8_t x_shift_letter = j_fonts_length[font_size] + J_KERNING; // How much we shift each letter by on the display
    if(len <= -1) { 
        len = 0;
        while(str[len]) len++; // Find length of the string
    }
    uint8_t x_shift = CENTERING ? ((len * x_shift_letter) / (3 - CENTERING)) : 0;


    x -= (x <= x_shift) ? 0 : x_shift;
    y = (2*y-j_fonts_height[font_size])/2;


    while(str[i] != 0 && i < J_MAX_PRINT_SIZE) {
        set_bounds(x,x+j_fonts_length[font_size]-1,y,y+j_fonts_height[font_size]-1);
        draw_text(str[i],font_size,color);
        x+= x_shift_letter;
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

void cnv_float_to_str(char* ret, float num) {
    int dec_after_point = (float)((int)num); // Truncate
    int dec_before_point = ((int)(num * ten_power))%ten_power; // Ten power is a variable that determines the degree of accuracy we want when converting decimals to lcd display data
    int i = 0;
    if(dec_after_point == 0) {
        ret[0] = '0';
        i = 1;
    } else {
        int number_cur; // Current remainder number
        while(i < (MAX_TEXT_TO_STRING_SIZE - MAX_DECIMAL_SIZE - 2) && dec_after_point) { // Truncate and modulo algorithm
            number_cur = dec_after_point % 10;
            dec_after_point /= 10;
            ret[i] = number_cur + 48;
            i++;
        }
        // Reversing the data
        char temp;
        for(int j = 0; j < i/2; j++) {
            temp = ret[j];
            ret[j] = ret[i - j - 1];
            ret[i - j - 1] = temp;
        }
    }
    // Now for the decimal part
    ret[i] = '.';
    ret[i+3] = 0;
    ret[i+2] = (dec_before_point % 10)+48;
    dec_before_point /= 10;
    ret[i+1] = dec_before_point + 48;
}



void fill_counter(j_counter* counter, uint16_t x, uint16_t y, float num, uint8_t font_size, j_color FILL_COL, j_centering CENTERING) {
    j_flag_set_bounds = 0;

    char ret_str[MAX_TEXT_TO_STRING_SIZE];
    int i, len;
    i = len = 0;
    cnv_float_to_str(ret_str, num);
    uint8_t x_shift_letter = j_fonts_length[font_size] + J_KERNING; // How much we shift each letter by on the display
    while(ret_str[len]) len++; // Find length of the string
    uint8_t x_shift = CENTERING ? ((len * x_shift_letter) / (3 - CENTERING)) : 0;

    x -= (x <= x_shift) ? 0 : x_shift;
    uint16_t y_mid, y_mid_end;
    y_mid = (2*y-j_fonts_height[font_size])/2;
    y_mid_end = y_mid+j_fonts_height[font_size];

    fill_text(x,y,ret_str,len,font_size,FILL_COL,J_LEFT);
    if(counter->start_x == -1) { // Uninitialized
        counter->start_x = x; 
        counter->end_x = x + len*(j_fonts_length[font_size]);
        counter->prev_val = num;
    } else { // Black box logic
        int mag1, mag2;
        mag1 = (int)floor(log10(fabs(num > 1 ? num : 1)));
        mag2 = (int)floor(log10(fabs(counter->prev_val > 1 ? counter->prev_val : 1)));

        uint8_t end_x_local = x + len*(j_fonts_length[font_size] + J_KERNING);
        if(mag1 < mag2) {
            set_bounds(x,counter->start_x,y_mid,y_mid_end);
            fill_area(j_master_control.bgcol);
            set_bounds(end_x_local,counter->end_x,y_mid,y_mid_end);
            fill_area(j_master_control.bgcol);
            // set_bounds(counter->start_x,counter->end_x,y_mid,y_mid_end);
            // fill_area(j_master_control.bgcol);
        }
        counter->prev_val = num;
        counter->start_x = x;
        counter->end_x = x + len*(j_fonts_length[font_size]);
    }
    // fill_text(x,y,ret_str,len,font_size,FILL_COL,J_LEFT);
}
