# Embedded-Journey
Code base for the Embedded-Journey YouTube channel. We bridge the gap between embedded hardware and firmware technology through hands-on tutorials.

# Featured Modules
es_Wifi : Production-ready drivers for the Inventek ISM43362/3 Wi-Fi series.
  - Utilizes the module's internal TCP stack via AT commands.
  - Includes wrappers for simplified socket management and TLS security.
  - es_wifi_io.c: have all the low level configuration for the board, SPI communication, read, write to the module.
  - es_wifi.c: all the parsing for the in/out AT command frames to the module.
  - wifi.c: warappers to the es_wifi functions, more usable for end users.
