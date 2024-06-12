#include "panel.hpp"
#include "ui.hpp"
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ili9342.h>
#include <esp_i2c.hpp>
#ifdef M5STACK_CORE2
#include <ft6336.hpp>
#endif
#ifdef M5STACK_TOUGH
#include <chsc6540.hpp>
#endif
#ifdef ARDUINO
using namespace arduino;
#else
using namespace esp_idf;
#endif
using namespace uix;
static esp_lcd_panel_handle_t lcd_handle;
uint8_t* panel_transfer_buffer1 = nullptr;
uint8_t* panel_transfer_buffer2 = nullptr;

static screen_t* lcd_active_screen = nullptr;
// for the touch panel
#ifdef M5STACK_CORE2
using touch_t = ft6336<320,280>;
#endif
#ifdef M5STACK_TOUGH
using touch_t = chsc6540<320,240,39>;
#endif
static touch_t touch(esp_i2c<1,21,22>::instance);


// tell UIX the DMA transfer is complete
static bool panel_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t* edata, void* user_ctx) {
    if(lcd_active_screen!=nullptr) {
        lcd_active_screen->flush_complete();
    }
    return true;
}
// tell the lcd panel api to transfer data via DMA
static void panel_on_flush(const rect16& bounds, const void* bmp, void* state) {
    int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2 + 1, y2 = bounds.y2 + 1;
    esp_lcd_panel_draw_bitmap(lcd_handle, x1, y1, x2, y2, (void*)bmp);
}

// for the touch panel
static void panel_on_touch(point16* out_locations,size_t* in_out_locations_size,void* state) {
    // UIX supports multiple touch points. so does the FT6336 so we potentially have
    // two values
    *in_out_locations_size = 0;
    uint16_t x,y;
    if(touch.xy(&x,&y)) {
        out_locations[0]=point16(x,y);
        ++*in_out_locations_size;
        if(touch.xy2(&x,&y)) {
            out_locations[1]=point16(x,y);
            ++*in_out_locations_size;
        }
    }
}

void panel_set_active_screen(screen_t& new_screen) {
    if(lcd_active_screen!=nullptr) {
        while(lcd_active_screen->flushing()) {
            vTaskDelay(5);
        }
        lcd_active_screen->on_flush_callback(nullptr);
        lcd_active_screen->on_touch_callback(nullptr);
    }
    new_screen.on_flush_callback(panel_on_flush);
    new_screen.on_touch_callback(panel_on_touch);
    lcd_active_screen=&new_screen;
    lcd_active_screen->invalidate();
}
void panel_update() {
    if(lcd_active_screen!=nullptr) {
        lcd_active_screen->update();    
    }
    // FT6336 chokes if called too quickly
    static uint32_t touch_ts = 0;
    if(pdTICKS_TO_MS(xTaskGetTickCount())>touch_ts+13) {
        touch_ts = pdTICKS_TO_MS(xTaskGetTickCount());
        touch.update();
    }
}

// initialize the screen using the esp panel API
void panel_init() {
    panel_transfer_buffer1 = (uint8_t*)heap_caps_malloc(panel_transfer_buffer_size,MALLOC_CAP_DMA);
    panel_transfer_buffer2 = (uint8_t*)heap_caps_malloc(panel_transfer_buffer_size,MALLOC_CAP_DMA);
    if(panel_transfer_buffer1==nullptr||panel_transfer_buffer2==nullptr) {
        puts("Out of memory allocating transfer buffers");
        while(1) vTaskDelay(5);
    }
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = 18;
    buscfg.mosi_io_num = 23;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = panel_transfer_buffer_size + 8;

    // Initialize the SPI bus on VSPI (SPI3)
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config;
    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = 15,
    io_config.cs_gpio_num = 5,
    io_config.pclk_hz = 40*1000*1000,
    io_config.lcd_cmd_bits = 8,
    io_config.lcd_param_bits = 8,
    io_config.spi_mode = 0,
    io_config.trans_queue_depth = 10,
    io_config.on_color_trans_done = panel_flush_ready;
    // Attach the LCD to the SPI bus
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle);

    lcd_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config;
    memset(&panel_config, 0, sizeof(panel_config));
    panel_config.reset_gpio_num = -1;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
#else
    panel_config.color_space = ESP_LCD_COLOR_SPACE_BGR;
#endif
    panel_config.bits_per_pixel = 16;

    // Initialize the LCD configuration
    esp_lcd_new_panel_ili9342(io_handle, &panel_config, &lcd_handle);

    // Reset the display
    esp_lcd_panel_reset(lcd_handle);

    // Initialize LCD panel
    esp_lcd_panel_init(lcd_handle);
    // esp_lcd_panel_io_tx_param(io_handle, LCD_CMD_SLPOUT, NULL, 0);
    //  Swap x and y axis (Different LCD screens may need different options)
    esp_lcd_panel_swap_xy(lcd_handle, false);
    esp_lcd_panel_set_gap(lcd_handle, 0, 0);
    esp_lcd_panel_mirror(lcd_handle, false, false);
    esp_lcd_panel_invert_color(lcd_handle, true);
    // Turn on the screen
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_lcd_panel_disp_on_off(lcd_handle, false);
#else
    esp_lcd_panel_disp_off(lcd_handle, false);
#endif
    touch.initialize();
    touch.rotation(0);
}