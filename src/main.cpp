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
#include <esp_i2c.hpp> // i2c initialization
#ifdef M5STACK_CORE2
#include <m5core2_power.hpp> // AXP192 power management (core2)
#endif
#ifdef M5STACK_TOUGH
#include <m5tough_power.hpp> // AXP192 power management (tough)
#endif
#include <bm8563.hpp> // real-time clock
#include <uix.hpp> // user interface library
#include <gfx.hpp> // graphics library
#include <wifi_manager.hpp> // wifi connection management
#include <ip_loc.hpp> // ip geolocation service
#include <ntp_time.hpp> // NTP client service
// font is a TTF/OTF from downloaded from fontsquirrel.com
// converted to a header with https://honeythecodewitch.com/gfx/converter
#define OPENSANS_REGULAR_IMPLEMENTATION
#include "assets/OpenSans_Regular.hpp" // our font
// icons generated using https://honeythecodewitch.com/gfx/iconPack
#define ICONS_IMPLEMENTATION
#include "assets/icons.hpp" // our icons
// include this after everything else except ui.hpp
#include "config.hpp" // time and font configuration
#include "ui.hpp" // ui declarations
#include "panel.hpp" // display panel functionality

// namespace imports
#ifdef ARDUINO
using namespace arduino; // libs (arduino)
#else
using namespace esp_idf; // libs (idf)
#endif
using namespace gfx; // graphics
using namespace uix; // user interface

#ifdef M5STACK_CORE2
using power_t = m5core2_power;
#endif
#ifdef M5STACK_TOUGH
using power_t = m5tough_power;
#endif
// for AXP192 power management
static power_t power(esp_i2c<1,21,22>::instance);

// for the time stuff
static bm8563 time_rtc(esp_i2c<1,21,22>::instance);
static char time_buffer[64];
static long time_offset = 0;
static ntp_time time_server;
static char time_zone_buffer[64];

// connection state for our state machine
typedef enum {
    CS_IDLE,
    CS_CONNECTING,
    CS_CONNECTED,
    CS_FETCHING,
    CS_POLLING
} connection_state_t;
static connection_state_t connection_state = CS_IDLE;

static wifi_manager wifi_man;

// the screen/control definitions
screen_t main_screen;
svg_clock_t ana_clock(main_screen);
label_t dig_clock(main_screen);
label_t time_zone(main_screen);
canvas_t wifi_icon(main_screen);
canvas_t battery_icon(main_screen);

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

static void wifi_icon_paint(surface_t& destination, 
                            const srect16& clip, 
                            void* state) {
    // if we're using the radio, indicate it 
    // with white. otherwise dark gray
    auto px = rgb_pixel<16>(3,6,3);
    const bool time_fetching = wifi_man.state()==wifi_manager_state::connecting || wifi_man.state()==wifi_manager_state::connected;
    if(time_fetching) {
        px = color_t::white;
    }
    draw::icon(destination,point16::zero(),faWifi,px);
}
static void battery_icon_paint(surface_t& destination, 
                                const srect16& clip, 
                                void* state) {
    // show in green if it's on ac power.
    int pct = power.battery_level();
    auto px = power.ac_in()?color_t::green:color_t::white;
   if(!power.ac_in() && pct<25) {
        px=color_t::red;
    }
    // draw an empty battery
    draw::icon(destination,point16::zero(),faBatteryEmpty,px);
    // now fill it up
    if(pct==100) {
        // if we're at 100% fill the entire thing
        draw::filled_rectangle(destination,rect16(3,7,22,16),px);
    } else {
        // otherwise leave a small border
        draw::filled_rectangle(destination,rect16(4,9,4+(0.18f*pct),14),px);
   }
}
#ifdef ARDUINO
void setup() {
    Serial.begin(115200);
    printf("Arduino version: %d.%d.%d\n",ESP_ARDUINO_VERSION_MAJOR,ESP_ARDUINO_VERSION_MINOR,ESP_ARDUINO_VERSION_PATCH);
#else
extern "C" void app_main() {
    printf("ESP-IDF version: %d.%d.%d\n",ESP_IDF_VERSION_MAJOR,ESP_IDF_VERSION_MINOR,ESP_IDF_VERSION_PATCH);
#endif
    power.initialize(); // do this first
    panel_init(); // do this next
    power.lcd_voltage(3.0);
    time_rtc.initialize();
    puts("Clock booted");
    if(power.charging()) {
        puts("Charging");
    } else {
        puts("Not charging"); // M5 Tough doesn't charge!?
    }
    // init the screen and callbacks
    main_screen.dimensions({320,240});
    main_screen.buffer_size(panel_transfer_buffer_size);
    main_screen.buffer1(panel_transfer_buffer1);
    main_screen.buffer2(panel_transfer_buffer2);
    main_screen.background_color(color_t::black);
    
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
    main_screen.register_control(ana_clock);

    // init the digital clock, (screen-width)x40, below the analog clock
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
    wifi_icon.on_touch_callback([](size_t locations_size, 
                                    const spoint16* locations, 
                                    void* state){
        if(connection_state==CS_IDLE) {
            connection_state = CS_CONNECTING;
        }
    },nullptr);
    main_screen.register_control(wifi_icon);
    
    // set up a custom canvas for displaying our battery icon
    battery_icon.bounds(
        (srect16)faBatteryEmpty.dimensions().bounds());
    battery_icon.on_paint_callback(battery_icon_paint);
    main_screen.register_control(battery_icon);
    
    panel_set_active_screen(main_screen);
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
        case CS_IDLE:
        if(connection_refresh_ts==0 || millis() > (connection_refresh_ts+
                                                    (time_refresh_interval*
                                                        1000))) {
            connection_refresh_ts = millis();
            connection_state = CS_CONNECTING;
        }
        break;
        case CS_CONNECTING:
        time_ts = 0; // for latency correction
        wifi_icon.invalidate(); // tell wifi_icon to repaint
        // if we're not in process of connecting and not connected:
        if(wifi_man.state()!=wifi_manager_state::connected && 
            wifi_man.state()!=wifi_manager_state::connecting) {
            puts("Connecting to network...");
            // connect
            wifi_man.connect(wifi_ssid,wifi_pass);
            connection_state =CS_CONNECTED;
        } else if(wifi_man.state()==wifi_manager_state::connected) {
            // if we went from connecting to connected...
            connection_state = CS_CONNECTED;
        }
        break;
        case CS_CONNECTED:
        if(wifi_man.state()==wifi_manager_state::connected) {
            puts("Connected.");
            connection_state = CS_FETCHING;
        } else if(wifi_man.state()==wifi_manager_state::error) {
            connection_refresh_ts = 0; // immediately try to connect again
            connection_state = CS_IDLE;
        }
        break;
        case CS_FETCHING:
        puts("Retrieving time info...");
        connection_refresh_ts = millis();
        // grabs the timezone offset based on IP
        if(!ip_loc::fetch(nullptr,
                            nullptr,
                            &time_offset,
                            nullptr,
                            0,
                            nullptr,
                            0,
                            time_zone_buffer,
                            sizeof(time_zone_buffer))) {
            // retry
            connection_state = CS_FETCHING;
            break;
        }
        time_ts = millis(); // we're going to correct for latency
        time_server.begin_request();
        connection_state = CS_POLLING;
        break;
        case CS_POLLING:
        if(time_server.request_received()) {
            const int latency_offset = ((millis()-time_ts)+500)/1000;
            time_rtc.set((time_t)(time_server.request_result()+
                            time_offset+latency_offset));
            puts("Clock set.");
            // set the digital clock - otherwise it only updates once a minute
            update_time_buffer(time_rtc.now());
            dig_clock.invalidate();
            time_zone.text(time_zone_buffer);
            connection_state = CS_IDLE;
            puts("Turning WiFi off.");
            wifi_man.disconnect(true);
            wifi_icon.invalidate();
        } else if(millis()>time_ts+(wifi_fetch_timeout*1000)) {
            puts("Retrieval timed out. Retrying.");
            connection_state = CS_FETCHING;
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
    if(power.ac_in()!=ac_in) {
        ac_in = power.ac_in();
        battery_icon.invalidate();
    }
    
    //////////////////////////
    // pump various objects
    /////////////////////////
    time_server.update();
    panel_update();
}
