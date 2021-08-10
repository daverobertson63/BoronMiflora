/*

    A BLE client for the Xiaomi Mi Plant Sensor, pushing measurements to an MQTT server.
    
    This is specifically for Particle Boron - as a cellular device which can connect to MQTT server
*/

#include <MQTT.h>

#include "Particle.h"
#include "secrets.h"

void callback(char *topic, byte *payload, unsigned int length);
MQTT client(MQTT_HOST, MQTT_PORT, callback);

// array of different xiaomi flora MAC addresses
char *FLORA_DEVICES[] = {
     "80:EA:CA:89:32:EA",
     "80:EA:CA:89:3B:A6",
     "80:EA:CA:89:33:1A",
     "80:EA:CA:89:37:DF"};

static int deviceCount = sizeof FLORA_DEVICES / sizeof FLORA_DEVICES[0];

FuelGauge fuel;

#define maxCharacteristics 30

BleCharacteristic sensorData;
BleCharacteristic writeMode;
BleCharacteristic batteryData;

// Sleep mode config
SystemSleepConfiguration config;

// Nice simple buffer for reading data values from sensor
byte char_data[32];

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
// SYSTEM_MODE(MANUAL);

SerialLogHandler logHandler(LOG_LEVEL_INFO);

const size_t SCAN_RESULT_MAX = 30;

BleScanResult scanResults[SCAN_RESULT_MAX];
BlePeerDevice peer;
bool sleepFlag = false;

int sleepHub(String command);

//OledWingAdafruit display;

uint16_t lastRate = 0;
// Used for the topic publisher
char buffer[64];

void onDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context);
void onSensorDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context);
void onWriteModeDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context);
void onBatteryDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context);

// this function automagically gets called upon a matching POST request
int sleepHub(String command)
{
    // look for the matching argument "coffee" <-- max of 64 characters long
    if (command == "awake")
    {
        sleepFlag = true;
        Log.info("Sleep Flag set to true - so staying awake");
        return 1;
    }
    else
    {
        sleepFlag = false;
        Log.info("Sleep Flag set to false - so will sleep on low power");
        return -1;
    }
}

void setup()
{

    (void)logHandler; // Does nothing, just to eliminate the unused variable warning

    Particle.variable("sleepHub", sleepFlag);

    Particle.function("sleepHub", sleepHub);

    Serial.begin();
    
    Serial.println("Active....");

    Log.info("Boron based MiFlora Sensor NLE Hub");
    BLE.on();

    config.mode(SystemSleepMode::ULTRA_LOW_POWER).duration(SLEEP_DURATION);

    sensorData.onDataReceived(onSensorDataReceived, NULL);
    writeMode.onDataReceived(onWriteModeDataReceived, NULL);
    batteryData.onDataReceived(onBatteryDataReceived, NULL);

    Log.info("Boron starting...");
}

void loop()
{

    int loopCount = 0;

    connectMqtt();
    client.subscribe("particle/boron/config/scantime");//color is the topic that photon is subscribed

    
    // Get configs
    
    // process devices - these should exist - maybe we get them from an external config firebase or sumthin
    for (int i = 0; i < deviceCount; i++)
    {

        // Need tp set up a new basetopic per dice :)

        int tryCount = 0;
        char *deviceMacAddress = FLORA_DEVICES[i];
        String baseTopic = MQTT_BASE_TOPIC + "/" + deviceMacAddress + "/";

        //BLEAddress floraAddress(deviceMacAddress);

        Log.info("Connecting to Miflora sensor %s", deviceMacAddress);
        peer = BLE.connect(BleAddress(deviceMacAddress));

        bool result = false;
        bool foundSensorData = false;

        if (peer.connected())
        {

            BleCharacteristic chars[maxCharacteristics];
            int foundChar = peer.discoverAllCharacteristics(chars, maxCharacteristics); // discover its exposed characteristics
            //Serial.printlnf("Found %d of %d characteristics", foundChar, maxCharacteristics);

            for (int c = 0; c < foundChar; c++)
            {
                //Serial.printlnf("%d. Characteristic: %s (%04x)", c + 1, (const char *)chars[c].UUID().toString(), chars[c].UUID().shorted());

                if (chars[c].UUID().shorted() == 0x1a00)
                {
                    result = peer.getCharacteristicByUUID(writeMode, BleUuid(0x1a00));
                    Log.info("writeMode Service %s", result ? "true" : "false");
                }
                else if (chars[c].UUID().shorted() == 0x1a01)
                {
                    result = peer.getCharacteristicByUUID(sensorData, BleUuid(0x1a01));
                    foundSensorData = true;
                    Log.info("SensorData Service %s", result ? "true" : "false");
                }
                else if (chars[c].UUID().shorted() == 0x1a02)
                {
                    result = peer.getCharacteristicByUUID(batteryData, BleUuid(0x1a02));
                    Log.info("batteryData Service %s", result ? "true" : "false");
                }
            }
            if (foundSensorData)
            {
                // We have found the service for the data - so we assume the write mode also.
                uint8_t writebuf[2] = {0xA0, 0x1F};
                writeMode.setValue(writebuf, 2);

                sensorData.getValue(char_data, 16);
                //Log.info("Raw Value  %x %x %x",char_data[0],char_data[1],char_data[2]);

                int16_t *temp_raw = (int16_t *)char_data;
                float temperature = (*temp_raw) / ((float)10.0);
                Log.info("-- Temperature: %f °C", temperature);

                int moisture = char_data[7];
                Log.info("-- Moisture: %d %%", moisture);

                int light = char_data[3] + char_data[4] * 256;
                Log.info("-- Light: %d lux", light);

                int conductivity = char_data[8] + char_data[9] * 256;
                Log.info("-- Soil Conductivity: %d uS/cm", conductivity);

                batteryData.getValue(char_data, 2);
                int battery = char_data[0];
                Log.info("-- Battery: %d %%", battery);

                if (temperature != 0 && temperature > -20 && temperature < 40)
                {
                    snprintf(buffer, 64, "%2.1f", temperature);
                    if (client.publish((baseTopic + "temperature").c_str(), buffer))
                    {
                        Log.info(" Topic: %s  Value: %s >> Published",(baseTopic + "temperature").c_str(),buffer);
                    }
                }
                else
                {
                    Log.info("   >> Skip temp publish");
                }
                
                delay(2000);

                if (moisture <= 100 && moisture >= 0)
                {
                    snprintf(buffer, 64, "%d", moisture);
                    if (client.publish((baseTopic + "moisture").c_str(), buffer))
                    {
                        Log.info(" Topic: %s  Value: %s >> Published",(baseTopic + "moisture").c_str(),buffer);
                    }
                }
                else
                {
                    Serial.println("   >> Skip moisture publish");
                }
                
                delay(2000);

                if (light >= 0)
                {
                    snprintf(buffer, 64, "%d", light);
                    if (client.publish((baseTopic + "light").c_str(), buffer))
                    {
                        Serial.println("   >> Published");
                    }
                }
                else
                {
                    Serial.println("   >> Skip");
                }
                
                delay(2000);

                if (conductivity >= 0 && conductivity < 5000)
                {
                    snprintf(buffer, 64, "%d", conductivity);
                    if (client.publish((baseTopic + "conductivity").c_str(), buffer))
                    {
                        Serial.println("   >> Published");
                    }
                }
                else
                {
                    Serial.println("   >> Skip");
                }
                
                delay(2000);

                snprintf(buffer, 64, "%d", battery);
                client.publish((baseTopic + "battery").c_str(), buffer);
                Serial.println("   >> Published battery");
            }

            Log.info("Sensor will now disconnect");
            peer.disconnect();

            snprintf(buffer, 64, "%s", "true");
        }
        else
        {
            Log.info("Sensor did not connect this time....");
            snprintf(buffer, 64, "%s", "false");
        }

        client.publish((baseTopic + "success"), buffer);

        delay(2000);
        snprintf(buffer, 64, "%.2f", fuel.getVCell());
        client.publish((baseTopic + "boronvoltage"), buffer);
        delay(2000);

        snprintf(buffer, 64, "%.2f", fuel.getSoC());
        client.publish((baseTopic + "boroncharge"), buffer);

        Log.info("voltage=%.2f", fuel.getVCell());
    }

    disconnectMqtt();

    Log.info("All devices processed - now determine wait strategy");

    if (sleepFlag)
    {
        Log.info("Stay Awake.... FFS - waiting for delay time in variable");
        delay(60000);
    }
    else
    {
        Log.info("We will shut down the device but wait 60 seconds");
        delay(60000);
        Log.info("Sleeping for 15 mins");
        if ( !sleepFlag) {
            System.sleep(config);
        }
    }
}

void onWriteModeDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context)
{
    uint8_t flags = data[0];

    Log.info("Writemode Data: Length  %d ", len);
}

void onSensorDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context)
{
    uint8_t flags = data[0];

    Log.info("Sensor Data: Length  %d ", len);

    int16_t *temp_raw = (int16_t *)data;
    float temperature = (*temp_raw) / ((float)10.0);
    Log.info("Callback Temperature: %f °C", temperature);
}

void onBatteryDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context)
{
    uint8_t flags = data[0];
    Log.info("Battery Data: Length  %d ", len);
}

void connectMqtt()
{

    Log.info("Connecting to MQTT... forever retry");

    while (!client.isConnected())
    {
        if (!client.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD))
        {
            Log.info("MQTT connection failed - password change");
            //Log.info(client.state());
            Log.info("Retrying...");
            delay(MQTT_RETRY_WAIT);
        }
    }

    Log.info("MQTT connected");
}

void disconnectMqtt()
{
    client.disconnect();
    Log.info("MQTT disconnected");
}

// recieve message junk
void callback(char *topic, byte *payload, unsigned int length)
{
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;

    Log.info("MQTT Callback payload %s", p);
}
