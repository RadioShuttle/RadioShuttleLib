![Title image](/docs/LoRa-zeichnung-4web_wifi-1.png)
# RadioShuttleLib
__Peer-to-peer LoRa wireless protocol software__
We developed a wireless protocol for peer to peer communication as well as server communication. The protocol is capable of efficiently sending messages in a fast , secure and reliable way, between simple radio modules (e.g. LoRa). The RadioShuttle protocol supports thousands of nodes, security via SHA256 based auhorization and AES encrypted messages. This software is equally suitable as a node or as a server. RadioShuttle protocol offers an easy to use  C++ API and turnkey sample applications for Arduino and Mbed OS.

We do not make use of the  LoRaWAN protocol because is lacks efficiency, does not support direct node-to-node communication, it is is too costly and complicated for many applications.

RadioShuttle integrates into standard MQTT environments via its MQTT Gateway (ESP32 ECO Power board). It also supports  MQTT push messages to mobile  clients via our MQTT Push Client App for Android and iOS.

We have a varity of RadioShuttle compaibtle boards available (LongRa, ECO Power and Turtle Boards). The source code allows users to get an insight of our protocol. We support custom board developments utilizing the RadioShuttle protocol, inquiries are welcome.

Helmut Tschemernjak
www.radioshuttle.de


## Prerequisites to test the RadioShuttle protocol
The easiest is to get a pair of the RadioShuttle supported boards to test the protocol. The solution includes hardware and software ready to go. Everyone (even non programmers) should be able to get example applications up and running. See details on www.radioshuttle.de

## Supported platforms
- Mbed OS (STM32L4, STM32L0 )
- Arduino (ESP32, D21)
- Linux planed
- Supported boards: LongRa , ECO Power and Turtle boards from Radioshuttle.de


## Supported radio drivers
The library depends on a common radio driver. At present the SX1276GenericLib has been used:
- https://github.com/RadioShuttle/SX1276GenericLib
Custom radio drivers for other radio chipsets can be developed when useing the same radio API as the SX1276GenericLib driver.


## TODOs
- xx


## License and contributions
Most of the software is provided under the Apache 2.0 license, see individual file headers. Contributions to this project are accepted under the same license. One protocol source file: ReadioShuttle.cpp  is under copyright of Helmut Tschemernjak and is subject of a license fee for any use. RadioShuttle boards customers are already licensed.


##  Credits
This protocol implementation has initially been written by the RadioShuttle engineers (www.radioshuttle.de). Many thanks to everyone who helped bringing this project forward.


##  Links
- RadioShuttle Web Site: https://www.radioshuttle.de/
- The RadioShuttle Wireless Protocol: https://www.radioshuttle.de/en/radioshuttle-en/protocol/
- LoRa basics: https://www.radioshuttle.de/en/lora-en/basics/
- MQTT basics: https://www.radioshuttle.de/en/mqtt-en/the-basics/
- MQTT Push Client (App) https://www.radioshuttle.de/en/mqtt-en/mqtt-push-client-app/
- Supported radio chips: https://github.com/RadioShuttle/SX1276GenericLib
