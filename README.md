# ESP32-Nixie-clock

<img width=40% height=auto alt="Nixie clock illustration" src="https://github.com/user-attachments/assets/72861d4f-c866-44fe-8231-edd676f40636" />

### A simple 2 tube Wi-Fi Nixie clock design
- built on perfboard with an ESP32, 74HC595 shift registers, CD4028BE BCD to DEC decoders and a bunch of high voltage transistors
- buttons and LEDs included!
- uses Wi-Fi to sync with RTC, **You need to specify your SSID and password of your network!** (see below)

## Features
- Type C connector
- HRs MINs and SECs selection
- ESP32 compatible
- Powerbank powered
- 100% not safe (HV of roughly 170VDC is exposed) for laymen
> The code is entirely vibe-coded but it works well!

## Connecting to Wi-Fi
- the ESP acts like a regular guest, so you need to specify credentials of the network you want the clock to be connected to
- you can do this by typing them into the upper part of the code
<img width="454" height="122" alt="obrazek" src="https://github.com/user-attachments/assets/28c677fb-bcfa-4102-bd03-1eb404297945" />
