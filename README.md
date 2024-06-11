# core2 clock example

- Entirely asynchronous
- Shuts off wifi when not in use
- Auto geolocation/timezone based on IP
- NTP for time
- Battery meter
- Supports ESP-IDF and Arduino
- Supports the M5 Stack Core 2 and the M5 Stack Tough**

** doesn't charge the battery for some reason on the Tough

In order to compile you'll need to add a wifi_creds.h file into the include/ folder.

```cpp
#ifndef WIFI_CREDS_H
#define WIFI_CREDS_H
#define WIFI_SSID "my_ssid"
#define WIFI_PASS "my_pass"
#endif
```