## Light lift firmware

### Configuration
After flashing the firmware, it must be configured before use if you want Home Assistant integration.

Connect to WiFi network LIGHT-XXXXXXXX (the name is derived from ESP32 MAC address) and navigate to http://192.168.4.1/. Password is `1234567890`, it can be changed in `magic.h`

Enter the credentials of your WiFi network and MQTT broker details, then hit **Save**. The system will reboot and try connect to WiFi and MQTT. You should see a new device in Home Assistant shortly. If that doe not happen, see ESP32 logs for troubleshooting hints.

### Remote config
The IR remote should use NEC encoding. Key definitions are in `magic.h`. If you need to change key mapping, you can examine ESP32 logs to see which codes your remote is sending.