#ifndef __H_MQTT_DISCOVERY__
#define __H_MQTT_DISCOVERY__

#include <AsyncUDP.h>
#include <AsyncMqttClient.h>

class MQTTDiscovery
{
public:
    MQTTDiscovery(uint16_t discoveryPort, uint16_t localPort, AsyncMqttClient *mqttClient);
    void discoverAndConnectBroker();

private:
    void onPacket(AsyncUDPPacket *packet);

private:
    uint16_t _discoveryPort;
    uint16_t _localPort;
    AsyncMqttClient *_mqttClient;
    AsyncUDP _udp;
};

#endif