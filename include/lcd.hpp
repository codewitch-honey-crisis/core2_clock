#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ui.hpp"
// use two 32KB buffers (DMA)
constexpr const size_t lcd_transfer_buffer_size = 32*1024;
extern uint8_t* lcd_transfer_buffer1;
extern uint8_t* lcd_transfer_buffer2;

void lcd_panel_init();

void lcd_update();

void lcd_set_active_screen(screen_t& new_screen);