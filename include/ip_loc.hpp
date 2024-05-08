#pragma once
#ifndef ESP32
#error "This library only supports the ESP32 MCU."
#endif
#include <Arduino.h>
namespace arduino {
struct ip_loc final {    
    static bool fetch(float* out_lat,
                    float* out_lon, 
                    long* out_utc_offset, 
                    char* out_region, 
                    size_t region_size, 
                    char* out_city, 
                    size_t city_size);
};
}