# Inswift-ETH Zigbee 3.0 PoE Ethernet USB Adapter's Firmware



## Firmware features

**Zigbee over LAN,USB,WiFI is supported.**

![img](https://private-user-images.githubusercontent.com/225222167/476363039-b8ed13d2-7e4d-4bda-bda7-46fcc696703a.png?jwt=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3NTY1NjIxMjQsIm5iZiI6MTc1NjU2MTgyNCwicGF0aCI6Ii8yMjUyMjIxNjcvNDc2MzYzMDM5LWI4ZWQxM2QyLTdlNGQtNGJkYS1iZGE3LTQ2ZmNjNjk2NzAzYS5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjUwODMwJTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI1MDgzMFQxMzUwMjRaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT0zODMzNjBjMDQzZTkwZmVlNDA3MjFhYWUyZGQ2MTU3YjA1ZWI2NGI0YjJlNzAzYzUxZTAxMjdhNTZlNjc1NGU4JlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCJ9.JJUyBYsIKFMDc38w1_HPOa7CNEbpXL__DCL6hhpluxg)

**1.SOC INFO**

used ESP32 + TI CC2652P/P7 or Silicon Labs MG21/24

Performance ranking: EFR32MG24 > EFR32MG21 > CC652P7 > CC2652P

Compatibility ranking: CC2562P > EFR32MG24 > EFR32MG21 > CC2652P7

High performance: EFR32MG24 supports multiple protocols such as Matter and Thread

**CC2652P：** 8MHz Cortex-M4 processor + 88KB RAM, the Zigbee firmware theoretically supports 200 Devices, with -105dBm sensitivity and +20dBm transmit power. It features balanced signal coverage and anti-interference capability, making it suitable for medium to large-scale networks. It requires configuration of "adapter: zstack", offers high cost-performance, and is fully supported by Zigbee2MQTT/ZHA.

**CC2652P7：** It has the same processor as the CC2652P (48MHz Cortex-M4), but the RAM is increased to 152KB, and it can theoretically handle more concurrent tasks. The firmware does not explicitly increase the device limit, but the large memory may reduce stuttering when multiple devices are running. It is similar to the CC2652P (-104dBm sensitivity), but supports advanced functions such as Mobile Target Detection (MTD). It needs to be configured with adapter: zstack, and is fully supported by Zigbee2MQTT/ZHA.

**EFR32MG21：** 80MHz Cortex-M33 processor + 64KB RAM, with computing power superior to TI chips. However, the smaller RAM may limit its performance in complex scenarios. It features a 20dBm transmit power and -104.3dBm sensitivity, and its measured link quality (LQI) is better than that of the CC26* series. It native supports Zigbee+Bluetooth and can serve as the core of a multi-protocol gateway.

**EFR32MG24：** 78MHz Cortex-M33 processor + 256KB RAM, boasting the strongest performance. It supports AI/ML acceleration and multiple protocols (Matter/Thread), with -105.4dBm sensitivity and 19.5dBm transmit power. It delivers optimal signal coverage and anti-interference capability, and supports hardware encryption (Secure Vault) and multiple peripherals, making it suitable for enterprise-level or complex IoT applications. [![image](https://private-user-images.githubusercontent.com/225222167/476295076-7df2ee49-3107-4df1-b24a-7d636fb26d1d.png?jwt=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3NTY1NjIzNjEsIm5iZiI6MTc1NjU2MjA2MSwicGF0aCI6Ii8yMjUyMjIxNjcvNDc2Mjk1MDc2LTdkZjJlZTQ5LTMxMDctNGRmMS1iMjRhLTdkNjM2ZmIyNmQxZC5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjUwODMwJTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI1MDgzMFQxMzU0MjFaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT1kMzAxYzg3NGU1N2Q2MzkwOGY3MzAzMDQ5ZTBlMTM0Y2UzMjk5NmVhNGIwY2Q0N2U5ZGI4OGQ1YzJlODlmZTQ4JlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCJ9.1snwY-GcwtXs4Z57po3lHkgEO5Fk4docHJTK62m2pUE)](https://private-user-images.githubusercontent.com/225222167/476295076-7df2ee49-3107-4df1-b24a-7d636fb26d1d.png?jwt=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3NTY1NjIzNjEsIm5iZiI6MTc1NjU2MjA2MSwicGF0aCI6Ii8yMjUyMjIxNjcvNDc2Mjk1MDc2LTdkZjJlZTQ5LTMxMDctNGRmMS1iMjRhLTdkNjM2ZmIyNmQxZC5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjUwODMwJTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI1MDgzMFQxMzU0MjFaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT1kMzAxYzg3NGU1N2Q2MzkwOGY3MzAzMDQ5ZTBlMTM0Y2UzMjk5NmVhNGIwY2Q0N2U5ZGI4OGQ1YzJlODlmZTQ4JlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCJ9.1snwY-GcwtXs4Z57po3lHkgEO5Fk4docHJTK62m2pUE)

**2.LED status**

BLUE - Working Mode zigbee usb status

GREEN - Working Mode zigbee network status

RED - Network link status

Cyan solid ：The device is starting up

Blue solid : USB to ZigBee mode

Blue blinking : Zigbee firmware is being updated

Green solid : ZigBeeMQTT has connected successfully

Green blinking slowly ：ZigBeeMQTT is not connected

Red solid ：Ethernet mode network disconnected

Off ： Be turned off / Device is not powered on



## Documentation

Visit [Wiki page](https://github.com/Inswift-dev/inswift-eth/wiki) to get information，There is guidance on product usage insid.



## Compiling from source

### VS Code
- You need npm and Python installed（Preferably Python 3.10.12 or higher）
- Install Visual Studio Code (VSC)  
- Install PlatformIO extension to VSC  
- Clone this repository  
- Open code-workspace in VSC
- Press "PlatformIO: Build" and wait until *.bin are generated  



## Contribute

Contributions are welcome! If you'd like to help improve the ETH Firmware, you can:

**Thank you very much [XZG](https://github.com/xyzroe/XZG) for providing open source for this project**

- Provide Pull Requests with enhancements or fixes. Please see our [contribution guidelines](CONTRIBUTING.md).
- Test newly released features and report issues.
- Help expand our documentation for better user support.

## Credits 


Special thanks to all third-party library authors. Their work has significantly contributed to this project:

- [espressif / arduino-esp32](https://github.com/espressif/arduino-esp32), 
- [esprfid / esp-rfid](https://github.com/esprfid/esp-rfid), 
- [fairecasoimeme / zigate-ethernet](https://github.com/fairecasoimeme/ZiGate-Ethernet), 
- [bblanchon / arduinojson](https://github.com/bblanchon/ArduinoJson), 
- [rLOGDacco / circularbuffer](https://github.com/rLOGDacco/CircularBuffer), 
- [sstaub / ticker](https://github.com/sstaub/Ticker), 
- [vurtun / lib](https://github.com/vurtun/lib),
- [Tinkerforge / WireGuard-ESP32-Arduino](https://github.com/Tinkerforge/WireGuard-ESP32-Arduino),  
- [sstaub / Ticker](https://github.com/sstaub/Ticker),
- [Martin-Laclaustra / CronAlarms](https://github.com/Martin-Laclaustra/CronAlarms),
- [xreef / WebServer-Esp8266-ESP32-Tutorial](https://github.com/xreef/WebServer-Esp8266-ESP32-Tutorial),
- [marvinroger / async-mqtt-client](https://github.com/marvinroger/async-mqtt-client)


## License

ETH Firmware is released under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for more details.

Third-party libraries used in this project are under their respective licenses. Please refer to each for more information.
