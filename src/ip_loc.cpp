#ifdef ESP32
#include <ip_loc.hpp>
#include <HTTPClient.h>
#include <json.hpp>
namespace arduino {
static char* ip_loc_fetch_replace_char(char* str, char find, char replace){
    char *current_pos = strchr(str,find);
    while (current_pos) {
        *current_pos = replace;
        current_pos = strchr(current_pos+1,find);
    }
    return str;
}
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
    char url[256];
    *url = 0;
    strcpy(url,"http://ip-api.com/json/?fields=status");//,region,city,lat,lon,timezone,offset";
    int count = 0;
    if(out_lat!=nullptr) {
        strcat(url,",lat");
        ++count;
    }
    if(out_lon!=nullptr) {
        strcat(url,",lon");
        ++count;
    }
    if(out_utc_offset!=nullptr) {
        strcat(url,",offset");
        ++count;
    }
    if(out_region!=nullptr && region_size>0) {
        strcat(url,",region");
        ++count;
    }
    if(out_city!=nullptr && city_size>0) {
        strcat(url,",city");
        ++count;
    }
    if(out_time_zone!=nullptr && time_zone_size>0) {
        strcat(url,",timezone");
        ++count;
    }

    HTTPClient client;
    client.begin(url);
    if(0>=client.GET()) {
        return false;
    }
    Stream& stm = client.getStream();
    io::arduino_stream astm(&stm);
    json::json_reader_ex<512> reader(astm);
    while(reader.read()) {
        if(reader.node_type()==json::json_node_type::field) {
            if(out_lat!=nullptr && 0==strcmp("lat",reader.value())) {
                reader.read();
                *out_lat = reader.value_real();
                --count;
            } else if(out_lon!=nullptr && 0==strcmp("lon",reader.value())) {
                reader.read();
                *out_lon = reader.value_real();
                --count;
            } else if(out_utc_offset!=nullptr && 0==strcmp("offset",reader.value())) {
                reader.read();
                *out_utc_offset = reader.value_int();
                --count;
            } else if(out_region!=nullptr && 0==strcmp("region",reader.value())) {
                reader.read();
                strncpy(out_region,reader.value(),region_size);
                --count;
            } else if(out_city!=nullptr && 0==strcmp("city",reader.value())) {
                reader.read();
                strncpy(out_city,reader.value(),city_size);
                --count;
            } else if(out_time_zone!=nullptr && 0==strcmp("timezone",reader.value())) {
                reader.read();
                strncpy(out_time_zone, reader.value(),time_zone_size);
                ip_loc_fetch_replace_char(out_time_zone,'_',' ');
                --count;
            }
        } else if(count<1 || reader.node_type()==json::json_node_type::end_object) {
            // don't wait for end of document to terminate the connection
            break;
        }
    }
    client.end();
    return true;
}
}
#endif