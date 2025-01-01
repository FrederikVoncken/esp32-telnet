# esp32-telnet

A telnet implementation for:
* The platform espressif32
* Framework espidf
* example board esp32doit-devkit-v1
* Visual Studio Code - PlatformIO

***

This implementation/example shows the use and implementation of telnet for esp32 idf environment.

There are 2 example implementations:
* A sendonly example, sending a string every second to all connected telnet applications
* An echo example, echoing all send characters from connected telnet applications

Example telnet applications I used are Putty (Windows) and ConnectBot (Android)

Configurations:
* Wifi_Config.h => This implementation does not have a WifiManager, so you need to input your WIFI credentials in this file
* Telnet_Config.h
  * TELNET_EXAMPLE_SEND_ONLY
    * Will enable the send only example
    * Disabling this define will enable the echo example
  * TELNET_MAX_SOCKET
    * Defines the maximum telnet connections to be active at the same time (e.g. 5 means max 5 applications connected)

***

Development links:
* https://platformio.org/
* https://docs.platformio.org/en/latest/platforms/espressif32.html

***

Thanks to:
* https://github.com/romix123/P1-wifi-gateway
* https://github.com/bartwo/esp32_p1meter
* https://github.com/daniel-jong/esp8266_p1meter

And the insights of:
* https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_server/main
* https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_server/main
* https://esp32.com/viewtopic.php?t=911