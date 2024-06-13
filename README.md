# core2 clock example

- Entirely asynchronous
- Shuts off wifi when not in use
- Auto geolocation/timezone based on IP
- NTP for time
- Battery meter
- Supports ESP-IDF and Arduino
- Supports the M5 Stack Core 2 and the M5 Stack Tough**

** doesn't charge the battery for some reason on the Tough

Article: https://www.codeproject.com/Articles/5383716/Core-2-Clock-A-dive-into-my-IoT-ecosystem


In order to compile you'll need to add a wifi_creds.h file into the include/ folder.

```cpp
#ifndef WIFI_CREDS_H
#define WIFI_CREDS_H
#define WIFI_SSID "my_ssid"
#define WIFI_PASS "my_pass"
#endif
```

Source contents

- include/wifi_creds.h: TO BE PROVIDED BY YOU - contains wifi credentials
- include/assets: embedded icons and fonts
- include/config.hpp: basic clock configuration
- include/panel.hpp: The display and touch panel header
- include/ui.hpp: The user interface declarations

- src/panel.cpp: The display and touch panel implementation
- src/main.cpp: The main application logic

- platformio.ini: The project configuration file
- 16MB.csv: set the Core2/Tough partitions to the correct settings