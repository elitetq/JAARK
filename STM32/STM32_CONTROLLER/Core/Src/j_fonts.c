#include "j_fonts.h"


static uint16_t* ret_arr;

const uint16_t* char_ptr_ret(char item, uint8_t font_size) {
    if(item < 32 || font_size >= 4 || j_fonts[font_size] == NULL) {
        return NULL;
    }
    return j_fonts[font_size] + (item - 32)*j_fonts_height[font_size];
}
