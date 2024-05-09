#ifdef ESP32
#include <ip_loc.hpp>
#include <HTTPClient.h>
#include <json.hpp>
namespace arduino {
/*bool ip_loc::fetch(float* out_lat,
                float* out_lon, 
                long* out_utc_offset, 
                char* out_region, 
                size_t region_size, 
                char* out_city, 
                size_t city_size,
                char* out_time_zone,
                size_t time_zone_size) {
    // URL for IP resolution service
    constexpr static const char* url = 
        "http://ip-api.com/csv/?fields=lat,lon,region,city,offset,timezone";
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
    str = stm.readStringUntil(',');
    if(out_time_zone!=nullptr && time_zone_size>0) {
         str.replace("_"," ");
         strncpy(out_time_zone,str.c_str(),min(str.length(),time_zone_size));
    }
    long lt = stm.parseInt();
    if(out_utc_offset!=nullptr) {
        *out_utc_offset = lt;
    }
    client.end();
    return true;
}*/
bool ip_loc::fetch(float* out_lat,
                float* out_lon, 
                long* out_utc_offset, 
                char* out_region, 
                size_t region_size, 
                char* out_city, 
                size_t city_size,
                char* out_time_zone,
                size_t time_zone_size) {
    // URL for IP resolution service
    constexpr static const char* url = 
        "http://ip-api.com/json/?fields=status,region,city,lat,lon,timezone,offset";
    HTTPClient client;
    client.begin(url);
    if(0>=client.GET()) {
        return false;
    }
    Stream& stm = client.getStream();
    io::arduino_stream astm(&stm);
    json::json_reader reader(astm);
    while(reader.read()) {
        if(reader.node_type()==json::json_node_type::field) {
            if(out_lat!=nullptr && 0==strcmp("lat",reader.value())) {
                reader.read();
                *out_lat = reader.value_real();
            } else if(out_lon!=nullptr && 0==strcmp("lon",reader.value())) {
                reader.read();
                *out_lon = reader.value_real();
            } else if(out_utc_offset!=nullptr && 0==strcmp("offset",reader.value())) {
                reader.read();
                *out_utc_offset = reader.value_int();
            } else if(out_region!=nullptr && 0==strcmp("region",reader.value())) {
                reader.read();
                strncpy(out_region,reader.value(),region_size);
            } else if(out_city!=nullptr && 0==strcmp("city",reader.value())) {
                reader.read();
                strncpy(out_city,reader.value(),city_size);
            } else if(out_time_zone!=nullptr && 0==strcmp("timezone",reader.value())) {
                reader.read();
                strncpy(out_time_zone,reader.value(),time_zone_size);
            }

        }
    }
    client.end();
    return true;
}
}
#endif