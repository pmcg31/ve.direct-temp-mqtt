#include <WiFi.h>
#include <AsyncUDP.h>
#include "mqtt_discovery.hpp"

const uint8_t magic[] = {0xde, 0xad, 0xfa, 0xce,
                         0xb0, 0x0b, 0x1e, 0xdd};
const size_t magicLen = 8;

MQTTDiscovery::MQTTDiscovery(uint16_t discoveryPort, uint16_t localPort, AsyncMqttClient *mqttClient)
    : _discoveryPort(discoveryPort), _localPort(localPort), _mqttClient(mqttClient)
{
}

void MQTTDiscovery::discoverAndConnectBroker()
{
    IPAddress myAddress = WiFi.localIP();

    uint8_t data[1024];
    size_t len = 0;
    for (int i = 0; i < magicLen; i++)
    {
        data[len++] = magic[i];
    }
    data[len++] = myAddress[0];
    data[len++] = myAddress[1];
    data[len++] = myAddress[2];
    data[len++] = myAddress[3];
    data[len++] = (_localPort >> 8) & 0x00ff;
    data[len++] = _localPort & 0x00ff;

    if (_udp.listen(_localPort))
    {
        _udp.onPacket([](void *arg, AsyncUDPPacket packet) {
            MQTTDiscovery *theObjectFormerlyKnownAsThis = (MQTTDiscovery *)arg;
            theObjectFormerlyKnownAsThis->onPacket(&packet);
        },
                      this);

        _udp.broadcastTo(data, len, _discoveryPort);
    }
}

void MQTTDiscovery::onPacket(AsyncUDPPacket *packet)
{
    uint8_t *data = packet->data();
    size_t dataLen = packet->length();

    if (dataLen == 14)
    {
        bool match = true;
        for (int i = 0; i < magicLen; i++)
        {
            if (data[i] != magic[i])
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            IPAddress brokerAddr = IPAddress(data[8], data[9], data[10], data[11]);
            uint16_t brokerPort = (data[12] << 8) + data[13];
            _mqttClient->setServer(brokerAddr, brokerPort);
            _mqttClient->connect();
        }
    }
}