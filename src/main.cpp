#include <Arduino.h>
#include <m5core2_power.hpp>
#include <bm8563.hpp>
#include <ft6336.hpp>
#include <tft_io.hpp>
#include <ili9341.hpp>
#include <uix.hpp>
#include <gfx.hpp>
#include <WiFi.h>
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
using namespace arduino;
using namespace gfx;
using namespace uix;

// for AXP192 power management (required for core2)
static m5core2_power power;

// for the LCD
using tft_bus_t = tft_spi_ex<VSPI,5,23,-1,18,0,false,32*1024+8>;
using lcd_t = ili9342c<15,-1,-1,tft_bus_t,1>;
static lcd_t lcd;
// use two 32KB buffers (DMA)
static uint8_t lcd_transfer_buffer1[32*1024];
static uint8_t lcd_transfer_buffer2[32*1024];

// for the touch panel
using touch_t = ft6336<280,320>;
static touch_t touch(Wire1);

// for the time stuff
static bm8563 time_rtc(Wire1);
static char time_buffer[32];
static long time_offset = 0;
static ntp_time time_server;
static bool time_fetching=false;

// the screen/control definitions
screen_t main_screen(
    {320,240},
    sizeof(lcd_transfer_buffer1),
    lcd_transfer_buffer1,
    lcd_transfer_buffer2);
svg_clock_t ana_clock(main_screen);
label_t dig_clock(main_screen);
canvas_t wifi_icon(main_screen);
canvas_t battery_icon(main_screen);

// for dumping to the display (UIX)
static void lcd_flush(const rect16& bounds,const void* bmp,void* state) {
    // wrap the void* bitmap buffer with a read only (const) bitmap object
    // this is a light and fast op
    const const_bitmap<decltype(lcd)::pixel_type> cbmp(bounds.dimensions(),bmp);
    // send what we just created to the display
    draw::bitmap_async(lcd,bounds,cbmp,cbmp.bounds());
}
// for display DMA (UIX/GFX)
static void lcd_wait_flush(void* state) {
    // wait for any async transfers to complete
    lcd.wait_all_async();
}
// for the touch panel
static void lcd_touch(point16* out_locations,size_t* in_out_locations_size,void* state) {
    // UIX supports multiple touch points. so does the FT6336 so we potentially have
    // two values
    *in_out_locations_size = 0;
    uint16_t x,y;
    if(touch.xy(&x,&y)) {
        Serial.printf("xy: (%d,%d)\n",x,y);
        out_locations[0]=point16(x,y);
        ++*in_out_locations_size;
        if(touch.xy2(&x,&y)) {
            Serial.printf("xy2: (%d,%d)\n",x,y);
            out_locations[1]=point16(x,y);
            ++*in_out_locations_size;
        }
    }
}
// updates the time string with the current time
static void update_time_buffer(time_t time) {
    tm tim = *localtime(&time);
    strftime(time_buffer, sizeof(time_buffer), "%I:%M %p", &tim);
    if(*time_buffer=='0') {
        *time_buffer=' ';
    }
}

static void wifi_icon_paint(surface_t& destination, const srect16& clip, void* state) {
    // if we're using the radio, indicate it with the appropriate icon
    if(time_fetching) {
        draw::icon(destination,point16::zero(),faWifi,color_t::light_gray);
    }
}
static void battery_icon_paint(surface_t& destination, const srect16& clip, void* state) {
    // display the appropriate icon for the battery level
    // show in green if it's on ac power.
    int pct = power.battery_level();
    auto px = power.ac_in()?color_t::green:color_t::white;
    const const_bitmap<alpha_pixel<8>>* ico;
    if(pct<25) {
        ico = &faBatteryEmpty;
        if(!power.ac_in()) {
            px=color_t::red;
        }
    } else if(pct<50) {
        ico = &faBatteryQuarter;
    } else if(pct<75) {
        ico = &faBatteryHalf;
    } else if(pct<100) {
        ico = &faBatteryThreeQuarters;
    } else {
        ico = &faBatteryFull;
    }
    draw::icon(destination,point16::zero(),*ico,px);
}

void setup()
{
    Serial.begin(115200);
    power.initialize(); // do this first
    lcd.initialize(); // do this next
    touch.initialize();
    touch.rotation(0);
    time_rtc.initialize();
    Serial.println("Clock booted");
    
    // init the screen and callbacks
    main_screen.background_color(color_t::black);
    main_screen.on_flush_callback(lcd_flush);
    main_screen.wait_flush_callback(lcd_wait_flush);
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
    main_screen.register_control(ana_clock);

    if(wifi_ssid!=nullptr) {
        Serial.print("Using fixed SSID and credentials: ");
        Serial.println(wifi_ssid);
    } else {
        Serial.println("Using stored SSID and credentials.");
    }
    
    // init the digital clock, 128x40, below the analog clock
    dig_clock.bounds(
        srect16(0,0,127,39)
            .center_horizontal(main_screen.bounds())
            .offset(0,128));
    update_time_buffer(time_rtc.now()); // prime the digital clock
    dig_clock.text(time_buffer);
    dig_clock.text_open_font(&text_font);
    dig_clock.text_line_height(35);
    dig_clock.text_color(color32_t::white);
    dig_clock.text_justify(uix_justify::top_middle);
    main_screen.register_control(dig_clock);

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
    main_screen.register_control(battery_icon);
}

void loop()
{
    ///////////////////////////////////
    // manage connection and fetching
    ///////////////////////////////////
    static int connection_state=0;
    static uint32_t connection_refresh_ts = 0;
    static uint32_t time_ts = 0;
    IPAddress time_server_ip;
    switch(connection_state) { 
        case 0: // idle
        if(connection_refresh_ts==0 || millis() > (connection_refresh_ts+(time_refresh_interval*1000))) {
            connection_refresh_ts = millis();
            connection_state = 1;
            time_ts = 0;
        }
        break;
        case 1: // connecting
            time_fetching = true;
            wifi_icon.invalidate();
            if(WiFi.status()!=WL_CONNECTED) {
                Serial.println("Connecting to network...");
                if(wifi_ssid==nullptr) {
                    WiFi.begin();
                } else {
                    WiFi.begin(wifi_ssid,wifi_pass);
                }
                connection_state =2;
            } else if(WiFi.status()==WL_CONNECTED) {
                connection_state = 2;
            }
            break;
        case 2: // connected
            if(WiFi.status()==WL_CONNECTED) {
                Serial.println("Connected.");
                connection_state = 3;
            } else if(WiFi.status()==WL_CONNECT_FAILED) {
                connection_refresh_ts = 0; // immediately try to connect again
                connection_state = 0;
                time_fetching = false;
            }
            break;
        case 3: // fetch
            Serial.println("Retrieving time info...");
            connection_refresh_ts = millis();
            // grabs the timezone offset based on IP
            ip_loc::fetch(nullptr,nullptr,&time_offset,nullptr,0,nullptr,0);
            WiFi.hostByName(time_server_domain,time_server_ip);
            connection_state = 4;
            time_ts = millis(); // we're going to correct for latency
            time_server.begin_request(time_server_ip);
            break;
        case 4: // polling for response
            if(time_server.request_received()) {
                const int latency_offset = (millis()-time_ts)/1000;
                time_rtc.set((time_t)(time_server.request_result()+time_offset+latency_offset));
                Serial.println("Clock set.");
                // set the digital clock - otherwise it only updates once a minute
                update_time_buffer(time_rtc.now());
                dig_clock.invalidate();
                connection_state = 0;
                Serial.println("Turning WiFi off.");
                WiFi.disconnect(true,false);
                time_fetching = false;
                wifi_icon.invalidate();
            } else if(millis()>time_ts+(wifi_fetch_timeout*1000)) {
                Serial.println("Retrieval timed out. Retrying.");
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
    if(millis()>touch_ts+50) {
        touch_ts = millis();
        touch.update();
    }
}
