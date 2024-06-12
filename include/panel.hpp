#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ui.hpp"
// use two 32KB buffers (DMA)
constexpr const size_t panel_transfer_buffer_size = 32*1024;
extern uint8_t* panel_transfer_buffer1;
extern uint8_t* panel_transfer_buffer2;

void panel_init();

void panel_update();

void panel_set_active_screen(screen_t& new_screen);