esp-sensor-mesh
===

*Warning: This is a hobby project and a work in progress.*

Software to build a sensor network consisting of just ESP8266 modules (these are 3$ wifi microcontrollers) and one or more wifi base stations at the edges. The ESP modules take readings from attached sensors, and forward the data to modules that are closer to the edge of the network. The modules at the edges forward the sensor readings to a server (for example an mqtt broker). This should allow you to build robust sensor networks stretching multiple kilometers at little cost.

Typical MESH networks are great on paper but difficult and error prone in practice. Why is this project different? By limiting the possibilities. Nodes can not talk to other nodes. Messaging is one directional. No addressing. The only supported operation is data flowing up towards the edge of the network, into the Cloud or whatever you have listening at the edge, as an exception on this, OTA is supported.


Design goals:
- self-reliant: power (solar?) + ESP8266 is all you need, no existing infrastructure
- one directional: sensor data should flow out of the network, no commands coming into the network (unless I find a way to do this, udp broadcast maybe?)
- OTA: despite the one directionality principle above, OTA should be possible
- dynamic: adding a module, an extra base station or removing a module will automatically trigger a network reconfiguration
- semi-reliable: during short periods of network reconfiguration, messages should not be lost (message delivery to the next hop is guaranteed)
- short messages: sensor readings should be less than 1000 bytes per message, this is not meant for sending photo or video as everything needs to be buffered in memory
- asynchronous: modules don't have a live tcp connection with the internet, they can just deliver data to the next hop
- scalable: when the network is saturated with messages, add more WiFi base stations at other corners of the network to scale up

Why:
- fun

How:
- modules are both access point and client ("station" in wifi terms) simultaneously
- each module advertises itself as ESPMESH-[Distance-to-edge]-[Firmware version]
- sensor traffic only flows upstream (towards the WiFi base stations)
- modules with a lower firmware can download the firmware from modules advertising a higher firmware

My setup:
- [Arduino IDE](https://github.com/esp8266/Arduino)
- [NodeMCU v2.0 dev boards](http://frightanic.com/iot/comparison-of-esp8266-nodemcu-development-boards/)

Disclaimer:
- this is my first ESP project
- this project is not affiliated with any ESP related company maker in any way (except that I bought the modules from one of them)


Help needed
===

I need help with getting OTA working, especially reading your own firmware and serving it as an HTTP response.


Technical details
===

Data flowing out of the network
---

Simplified view, the SSIDs only have the distance to the base network, not the firmware version. The arrows show how sensor data is flowing out of the network:

![sensor data out of network](readme/mesh-1.png)

OTA (Over the Air updates)
---

If you are serious about IOT you know that automatic OTA updates are a must, both to shorten development times and for flexibility in production deployments.

OTA updates flow through the network like a virus. Simply update one module which will advertise the new firmware in the SSID (firmwares are numbered sequentially), in this case the firmware of the module in the bottom is bumped from `v0` to `v1`:

![upgrade one firmware manually](readme/mesh-with-firmware-1.png)

All nearby modules with a lower firmware number will connect to the network of a higher firmware, download its firmware and apply it to itself:

![nearby nodes will connect and download it's firmware](readme/mesh-with-firmware-2.png)

Now the next modules will start the update process:

![firmware is spread to 3 nodes now, the rest will follow](readme/mesh-with-firmware-3.png)

In the diagram above the upper nodes won't be reached but that's a detail.


Basic algorithm
---

D = distance from the edge
N = firmware version

If board sees a (hardcoded) base station (such as your home wifi router), it will connect to the base station and advertise itself as `ESPMESH-0-[N]`.

If no base station seen, it will connect to the ESPMESH node with the lowest advertised distance to the edge (D). It will advertise itself as `ESPMESH-[D + 1]-[N]`.

Each module will set up an internal network `192.168.D.1/24`. An HTTP server listens on traffic from connected clients (which have a higher D value) and forwards it to nodes with a lower D value, or if at the edge, to a (hardcoded in your firmware) url on your home network or on the Internet.
