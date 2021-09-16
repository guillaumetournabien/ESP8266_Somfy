# ESP8266_Somfy rev 2.01

# From original Somfy ESP8266 Remote Arduino rev 0.1
Somfy ESP8266 Remote 433.24 RTS

Description: Lightweight arduino code for an esp8266 NodeMCU (lolin)

This is a reverse engeneered 433 transmitter for Somfy 433 shutters, controlling the shutter without custumizations on or in the device itself.

This project is an forke of made in Platform IO

https://github.com/marmotton/Somfy_Remote

He forked it from 

https://github.com/Nickduino/Somfy_Remote.

If you want to learn more about the Somfy RTS protocol, check out Pushtack.

**Usage of this project is on your own risk.**

# Motivation
HAve a central remote multichannel for all Somfy RTS remote

My requirements would be:
- Multichannel version up to 16 remote controler
- Wifi Assistant for easy configuration (based on portail after each reboot)
- Usable with Home Assistant
- Integrates with MQTT
- Has state reporting
- ESP8266 NodeMCU (lolin compatible)
- A tight PCB for industry finish

# Changes
1) add wifi_manager to configure the module via Wifi Access point
2) Add 16 remotes
3) Be able to reprogramme the ruling code for easy update
4) Full MQTT remote
   - list of command:
    - up
    - down
    - stop
    - prog
    - reboot
    - reset_code (fix rulling code to 1)
    - read_code (read the current rulling code of the selected channel)
    - write_code (write the current rulling code of the selected channel)

# Requirements:
- MQTT bus
- Home Assistant
- Arduino IDE
- ESP8266
- Soldering iron (fine skills to desolder and solder)
- 433 transmitter
- Led inidicator
- resistor of 330ohm
- 2x 15 pin headers
- Short 433 antenna
- Somfy RT

# Desolder 433.92Mhz and solder 433.42Mhz crystal on transmitter
So firstly remove the original crystal with the new 433.42Mhz (see list below).
Tip, there is a pin on the side as marking to connect the new one in the correct order (if your unsure)

# Order PCB
See this site: https://aisler.net/partners/fritzing 
You can order with this file: https://github.com/RoyOltmans/somfy_esp8266_remote_arduino/raw/master/esp%20somfy%20rf_v1.2.fzz

# Assembling
tips: 
- You will need to use an resistor of 330ohm on the pin of the led if you want a indicator that the transmission is working
- Use a 15pin header instead of direct soldering the ESP to the board (reusability and the possibiltiy of damaging the ESP)
- You will also need to clip the pins on the back of the transmitter because the design is a little to tight

# Flash with Arduino
Firstly use the standard libraries and at least version 1.8.9 of Arduino

Edit the config.h to match your environment (read previous forkes for some extra info)

Change:
- [SSID] (remove brackets)
- [SSID Password] (remove brackets)
- IMPORTANT Change any accurance of [DEVICE ID YOU USE] 4 times int he file (remove brackets), use a string without special characters eg "abcd" not "a,b;c:/d"
- [MQTT HOST Server] (remove brackets)

See for example this site (have not tested this one):
http://henrysbench.capnfatz.com/henrys-bench/arduino-projects-tips-and-more/arduino-esp8266-lolin-nodemcu-getting-started/

# Usage MQTT
I prosume you have installed mosquitto (any other mqtt client is ok)

The ESP will subscribe to the configured MQTT topics. Watch what is happening on the serial port to make sure it is working.

mosquitto_sub -h [MQTT Server HOST] -t '#' -v

The following commands will work:
Shutter UP
```
mosquitto_pub -h server-01 -t 'home/nodemcu/somfy/attic_blinds' -m 'u'
```

Shutter STOP
```
mosquitto_pub -h server-01 -t 'home/nodemcu/somfy/attic_blinds' -m 's'
```

Shutter DOWN
```
mosquitto_pub -h server-01 -t 'home/nodemcu/somfy/attic_blinds' -m 'd'
```

Programming the blinds:

Press the program button on your actual remote. The blinds will move slightly.
Publish 'p' message on the corresponding MQTT topic. The blinds will move slightly.
Done !

Somfy PROGRAMING
```
mosquitto_pub -h server-01 -t 'home/nodemcu/somfy/attic_blinds' -m 'p'
```

The rolling code value is stored in the EEPROM, so that you don't loose count of your rolling code after a reset. In case you'd like to replace the ESP, write down the current rolling codes which can be read using the serial terminal (and use them as default rolling codes in config.h).

If all goes well you have an working Somfy set

# Home Assistant
Under covers in HA, use this config
```
  - platform: mqtt
    name: "Somfy Shutter"
    availability_topic: "somfy_remote/[DEVICE ID YOU USE]/status"
    state_topic: "somfy_remote//[DEVICE ID YOU USE]/state"
    command_topic: "somfy_remote//[DEVICE ID YOU USE]"
    qos: 1
    payload_open: "up"
    payload_stop: "stop"
    payload_close: "down"
    payload_prog: "prog"
    retain: true
```

## The hardware list is as followed (made by Marmotton):
The [doc](https://github.com/marmotton/Somfy_Remote/blob/master/doc) folder contains some photos.
- ESP8266 board: [AliExpress link](https://www.aliexpress.com/item/D1-mini-Mini-NodeMcu-4M-bytes-Lua-WIFI-Internet-of-Things-development-board-based-ESP8266-by/32633763949.html)
- 433MHz RF transmitter: [AliExpress link](https://www.aliexpress.com/item/433Mhz-RF-Transmitter-and-Receiver-Module-Link-Kit-for-ARM-MCU-WL-DIY-315MHZ-433MHZ-Wireless/32298304710.html)
- 433.42MHz quartz: [eBay link](https://www.ebay.ch/itm/5PCS-433-42M-433-42MHz-R433-F433-SAW-Resonator-Crystals-TO-39-NEW/232574365405)
- Housing: [Youmagine link](https://www.youmagine.com/designs/housing-for-a-d1-mini-with-rf)
