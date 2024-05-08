#ifdef ESP32
#include <ip_loc.hpp>
#include <HTTPClient.h>
namespace arduino {
bool ip_loc::fetch(float* out_lat,
                float* out_lon, 
                long* out_utc_offset, 
                char* out_region, 
                size_t region_size, 
                char* out_city, 
                size_t city_size) {
    // URL for IP resolution service
    constexpr static const char* url = 
        "http://ip-api.com/csv/?fields=lat,lon,region,city,offset";
    HTTPClient client;
    client.begin(url);
    if(0>=client.GET()) {
        return false;
    }
    Stream& stm = client.getStream();

    String str = stm.readStringUntil(',');
    int ch;
    if(out_region!=nullptr && region_size>0) {
        strncpy(out_region,str.c_str(),min(str.length(),region_size));
    }
    str = stm.readStringUntil(',');
    if(out_city!=nullptr && city_size>0) {
        strncpy(out_city,str.c_str(),min(str.length(),city_size));
    }
    float f = stm.parseFloat();
    if(out_lat!=nullptr) {
        *out_lat = f;
    }
    ch = stm.read();
    f = stm.parseFloat();
    if(out_lon!=nullptr) {
        *out_lon = f;
    }
    ch = stm.read();
    long lt = stm.parseInt();
    if(out_utc_offset!=nullptr) {
        *out_utc_offset = lt;
    }
    client.end();
    return true;
}
}
#endif