#if __has_include(<Arduino.h>)
#include <Arduino.h>
#else
#include <freertos/FreeRTOS.h>
#include <stdint.h>
void loop();
static uint32_t millis() {
    return pdTICKS_TO_MS(xTaskGetTickCount());
}
#endif
#include <esp_i2c.hpp>
#ifdef M5STACK_CORE2
#include <m5core2_power.hpp>
#endif
#ifdef M5STACK_TOUGH
#include <m5tough_power.hpp>
#endif
#include <bm8563.hpp>
#ifdef M5STACK_CORE2
#include <ft6336.hpp>
#endif
#ifdef M5STACK_TOUGH
#include <chsc6540.hpp>
#endif
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ili9342.h>

#include <uix.hpp>
#include <gfx.hpp>
#include <wifi_manager.hpp>
#include <ip_loc.hpp>
#include <ntp_time.hpp>
// font is a TTF/OTF from downloaded from fontsquirrel.com
// converted to a header with https://honeythecodewitch.com/gfx/converter
#define OPENSANS_REGULAR_IMPLEMENTATION
#include <assets/OpenSans_Regular.hpp>
// faWifi icon generated using https://honeythecodewitch.com/gfx/iconPack
#define ICONS_IMPLEMENTATION
#include <assets/icons.hpp>
// include this after everything else except ui.hpp
#include <config.hpp>
#include <ui.hpp>

// namespace imports
#ifdef ARDUINO
using namespace arduino;
#else
using namespace esp_idf;
#endif
using namespace gfx;
using namespace uix;

#ifdef M5STACK_CORE2
// for AXP192 power management
static m5core2_power power(esp_i2c<1,21,22>::instance);
#endif

#ifdef M5STACK_TOUGH
// for AXP192 power management
static m5tough_power power(esp_i2c<1,21,22>::instance);
#endif

esp_lcd_panel_handle_t lcd_handle;
// use two 32KB buffers (DMA)
static uint8_t lcd_transfer_buffer1[32*1024];
static uint8_t lcd_transfer_buffer2[32*1024];

#ifdef M5STACK_CORE2
// for the touch panel
using touch_t = ft6336<320,280>;
#endif
#ifdef M5STACK_TOUGH
using touch_t = chsc6540<320,240,39>;
#endif

static touch_t touch(esp_i2c<1,21,22>::instance);

wifi_manager wifi_man;

// for the time stuff
static bm8563 time_rtc(esp_i2c<1,21,22>::instance);
static char time_buffer[64];
static long time_offset = 0;
static ntp_time time_server;
static char time_zone_buffer[64];
static bool time_fetching=false;

static int connection_state = 0;

// the screen/control definitions
screen_t main_screen(
    {320,240},
    sizeof(lcd_transfer_buffer1),
    lcd_transfer_buffer1,
    lcd_transfer_buffer2);
svg_clock_t ana_clock(main_screen);
label_t dig_clock(main_screen);
label_t time_zone(main_screen);
canvas_t wifi_icon(main_screen);
canvas_t battery_icon(main_screen);

// tell UIX the DMA transfer is complete
static bool lcd_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t* edata, void* user_ctx) {
    main_screen.flush_complete();
    return true;
}
// tell the lcd panel api to transfer data via DMA
static void lcd_on_flush(const rect16& bounds, const void* bmp, void* state) {
    int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2 + 1, y2 = bounds.y2 + 1;
    esp_lcd_panel_draw_bitmap(lcd_handle, x1, y1, x2, y2, (void*)bmp);
}

// for the touch panel
static void lcd_touch(point16* out_locations,size_t* in_out_locations_size,void* state) {
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
// initialize the screen using the esp panel API
static void lcd_panel_init() {
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = 18;
    buscfg.mosi_io_num = 23;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = sizeof(lcd_transfer_buffer1) + 8;

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
    io_config.on_color_trans_done = lcd_flush_ready;
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
}
// updates the time string with the current time
static void update_time_buffer(time_t time) {
    char sz[64];
    tm tim = *localtime(&time);
    *time_buffer = 0;
    strftime(sz, sizeof(sz), "%D ", &tim);
    strcat(time_buffer,sz);
    strftime(sz, sizeof(sz), "%I:%M %p", &tim);
    if(*sz=='0') {
        *sz=' ';
    }
    strcat(time_buffer,sz);
}

static void wifi_icon_paint(surface_t& destination, const srect16& clip, void* state) {
    // if we're using the radio, indicate it with the appropriate icon
    auto px = rgb_pixel<16>(3,7,3);
    if(time_fetching) {
        px = color_t::white;
    }
    draw::icon(destination,point16::zero(),faWifi,px);
}
static void battery_icon_paint(surface_t& destination, const srect16& clip, void* state) {
    // show in green if it's on ac power.
    int pct = power.battery_level();
    auto px = power.ac_in()?color_t::green:color_t::white;
   if(!power.ac_in() && pct<25) {
        px=color_t::red;
    }
    draw::icon(destination,point16::zero(),faBatteryEmpty,px);
    if(pct==100) {
        draw::filled_rectangle(destination,rect16(3,7,22,16),px);
    } else {
        draw::filled_rectangle(destination,rect16(4,9,4+(0.18f*pct),14),px);
   }
    
}
#ifdef ARDUINO
void setup()
{
    Serial.begin(115200);
#else
extern "C" void app_main() {
#endif
    power.initialize(); // do this first
    lcd_panel_init(); // do this next
    power.lcd_voltage(3.0);
    touch.initialize();
    touch.rotation(0);
    time_rtc.initialize();
    puts("Clock booted");
    if(power.charging()) {
        puts("Charging");
    } else {
        puts("Not charging");
    }
    // init the screen and callbacks
    main_screen.background_color(color_t::black);
    main_screen.on_flush_callback(lcd_on_flush);
    main_screen.on_touch_callback(lcd_touch);

    // init the analog clock, 128x128
    ana_clock.bounds(srect16(0,0,127,127).center_horizontal(main_screen.bounds()));
    ana_clock.face_color(color32_t::light_gray);
    // make the second hand semi-transparent
    auto px = ana_clock.second_color();
    // use pixel metadata to figure out what half of the max value is
    // and set the alpha channel (A) to that value
    px.template channel<channel_name::A>(
        decltype(px)::channel_by_name<channel_name::A>::max/2);
    ana_clock.second_color(px);
    // do similar with the minute hand as the second hand
    px = ana_clock.minute_color();
    // same as above, but it handles it for you, using a scaled float
    px.template channelr<channel_name::A>(0.5f);
    ana_clock.minute_color(px);
    // make the whole thing dark
    ana_clock.hour_border_color(color32_t::gray);
    ana_clock.minute_border_color(ana_clock.hour_border_color());
    ana_clock.face_color(color32_t::black);
    ana_clock.face_border_color(color32_t::black);
    //ana_clock.tick_color(color32_t::black);
    main_screen.register_control(ana_clock);

    // init the digital clock, 128x40, below the analog clock
    dig_clock.bounds(
        srect16(0,0,main_screen.bounds().x2,39)
            .offset(0,128));
    update_time_buffer(time_rtc.now()); // prime the digital clock
    dig_clock.text(time_buffer);
    dig_clock.text_open_font(&text_font);
    dig_clock.text_line_height(35);
    dig_clock.text_color(color32_t::white);
    dig_clock.text_justify(uix_justify::top_middle);
    main_screen.register_control(dig_clock);

    const uint16_t tz_top = dig_clock.bounds().y1+dig_clock.dimensions().height;
    time_zone.bounds(srect16(0,tz_top,main_screen.bounds().x2,tz_top+40));
    time_zone.text_open_font(&text_font);
    time_zone.text_line_height(30);
    time_zone.text_color(color32_t::light_sky_blue);
    time_zone.text_justify(uix_justify::top_middle);
    main_screen.register_control(time_zone);

    // set up a custom canvas for displaying our wifi icon
    wifi_icon.bounds(
        srect16(spoint16(0,0),(ssize16)wifi_icon.dimensions())
            .offset(main_screen.dimensions().width-
                wifi_icon.dimensions().width,0));
    wifi_icon.on_paint_callback(wifi_icon_paint);
    main_screen.register_control(wifi_icon);
    
    // set up a custom canvas for displaying our battery icon
    battery_icon.bounds(
        (srect16)faBatteryEmpty.dimensions().bounds());
    battery_icon.on_paint_callback(battery_icon_paint);
    wifi_icon.on_touch_callback([](size_t locations_size, const spoint16* locations, void* state){
        if(connection_state==0) {
            connection_state = 1;
        }
    },nullptr);
    main_screen.register_control(battery_icon);
    rgba_pixel<32> transparent(0,0,0,0);

#ifndef ARDUINO
    while(1) {
        loop();
        vTaskDelay(5);
    }
#endif
}

void loop()
{
    ///////////////////////////////////
    // manage connection and fetching
    ///////////////////////////////////
    static uint32_t connection_refresh_ts = 0;
    static uint32_t time_ts = 0;
    switch(connection_state) { 
        case 0: // idle
        if(connection_refresh_ts==0 || millis() > (connection_refresh_ts+(time_refresh_interval*1000))) {
            connection_refresh_ts = millis();
            connection_state = 1;
        }
        break;
        case 1: // connecting
            time_ts = 0;
            time_fetching = true;
            wifi_icon.invalidate();
            if(wifi_man.state()!=wifi_manager_state::connected && wifi_man.state()!=wifi_manager_state::connecting) {
                puts("Connecting to network...");
                wifi_man.connect(wifi_ssid,wifi_pass);
                connection_state =2;
            } else if(wifi_man.state()==wifi_manager_state::connected) {
                connection_state = 2;
            }
            break;
        case 2: // connected
            if(wifi_man.state()==wifi_manager_state::connected) {
                puts("Connected.");
                connection_state = 3;
            } else if(wifi_man.state()==wifi_manager_state::error) {
                connection_refresh_ts = 0; // immediately try to connect again
                connection_state = 0;
                time_fetching = false;
            }
            break;
        case 3: // fetch
            puts("Retrieving time info...");
            connection_refresh_ts = millis();
            // grabs the timezone offset based on IP
            ip_loc::fetch(nullptr,nullptr,&time_offset,nullptr,0,nullptr,0,time_zone_buffer,sizeof(time_zone_buffer));
            connection_state = 4;
            time_ts = millis(); // we're going to correct for latency
            time_server.begin_request();
            break;
        case 4: // polling for response
            if(time_server.request_received()) {
                const int latency_offset = (millis()-time_ts)/1000;
                time_rtc.set((time_t)(time_server.request_result()+time_offset+latency_offset));
                puts("Clock set.");
                // set the digital clock - otherwise it only updates once a minute
                update_time_buffer(time_rtc.now());
                dig_clock.invalidate();
                time_zone.text(time_zone_buffer);
                connection_state = 0;
                puts("Turning WiFi off.");
                wifi_man.disconnect(true);
                time_fetching = false;
                wifi_icon.invalidate();
            } else if(millis()>time_ts+(wifi_fetch_timeout*1000)) {
                puts("Retrieval timed out. Retrying.");
                connection_state = 3;
            }
            break;
    }
    ///////////////////
    // update the UI
    //////////////////
    time_t time = time_rtc.now();
    ana_clock.time(time);
    // only update every minute (efficient)
    if(0==(time%60)) {
        update_time_buffer(time);
        // tell the label the text changed
        dig_clock.invalidate();
    }
    // update the battery level
    static int bat_level = power.battery_level();
    if((int)power.battery_level()!=bat_level) {
        bat_level = power.battery_level();
        battery_icon.invalidate();
    }
    static bool ac_in = power.ac_in();
    if((int)power.battery_level()!=ac_in) {
        ac_in = power.ac_in();
        battery_icon.invalidate();
    }
    
    //////////////////////////
    // pump various objects
    /////////////////////////
    time_server.update();
    main_screen.update();    
    // FT6336 chokes if called too quickly
    static uint32_t touch_ts = 0;
    if(millis()>touch_ts+13) {
        touch_ts = millis();
        touch.update();
    }
}
