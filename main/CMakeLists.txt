idf_component_register(
    SRCS 
    "http_server.c" 
    "light.c"
    "leds.c"
    "wifi.c"
    "messaging.c"
    "remote.c"
    "tilt_sensor.c"
    "motors.c"
    "mqtt.c"
    "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "files/index.html" "files/config_payload.json"
)
