# BoronMiflora
Particle Boron and Miflora Sensors with MQTT

Particle compatible sketch as a client for Xiaomi Mi Flora Plant sensors. Boron can be powered with LiPo battery and battery level is read. Battery level and Mi Flora sensor measurements are pushed to the MQTT server.  MQTT can then be used for Home Assistant - or anything else you desire.

## Background

The background to this project is based on a number of existing projects for Arduino or ESP32 and very good they are.  The way we process the miflora devices is the same - you already know the BLE address and you essentially loop round looking for these devices.  What I found out was that the distance from the ESP or Arduino needs to be quite close and if you are doing this inside - ESP32 is a better way - cheaper as it only needs WIFI.  However I wanted to use the Boron because that uses GSM and I can drive that for outside plants.  I also attached this to a Solar power charger and as such it works outside without needing access to power. So this is the Boron version. Particle devs will know this well. 

## Hardware & Software used

- Particle Boron -  https://docs.particle.io/boron/
- Adafruit Lithium Ion Battery 3.7v 2000mAh - https://www.adafruit.com/product/2011
- Xiaomi Mi Plant Sensor
- MQTT server: Home Assistant Mosquitto broker add-on - https://home-assistant.io
- CloudMQTT - https://www.cloudmqtt.com/

## Setup

1. Edit these in either miflorable.ino - or secrets.h
- FLORA_DEVICES - MAC address(es) of your Xiaomi Mi Plant sensor(s)
- SLEEP_DURATION - sleep duration between sensor reads in seconds 
- MQTT_RETRY_WAIT - Try to connect to the MQTT Server - without it is pointless.
- MQTT_HOST:  Hostname for MQTT
- MQTT_PORT: Port number 
-  MQTT_CLIENTID = "miflora-boron-client";
- MQTT_USERNAME: Username for MQTT
- MQTT_PASSWORD: Password for MQTT
- MQTT_BASE_TOPIC = "flora"; 

2. Open sketch in console.particle.io - or use local compile and buid
3. boron_configuration.yaml - Example MQTT sensor config for Home Assistant (replace MAC addresses in the file for your MiFlora - note that the boron charge is the mac address of the boron)

## To Do....

For Homeassistant - it would be better to publish the auto configure - where you can simple publish the configs as entities.  I would also want to rengineer the BLW code so that it can auto discover any miflora device - and just publish it.  Would make more sense so you dont need to push a new code block. 

## Credits

- Original arduino sketch - https://github.com/sidddy/flora
- More ideas for the sketch - https://github.com/Pi-And-More/PAM-ESP32-Multi-MiFlora
- This is based in this one  - https://github.com/e6on/ESP32_MiFlora_MQTT