#ifndef __H_CONFIG__
#define __H_CONFIG__

#include <SPIFFS.h>

class Config
{
public:
    Config();

public:
    bool readConfig(File configFile);

    const char *getSSID();
    const char *getKey();
    const char *getMDNS();

private:
    DynamicJsonDocument _doc;
};

#endif