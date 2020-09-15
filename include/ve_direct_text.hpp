#ifndef __H_VE_DIRECT_TEXT__
#define __H_VE_DIRECT_TEXT__

#include <SPIFFS.h>
#include <ArduinoJson.h>

#define MAX_ERROR_LEN 2048
#define MAX_VIC_PAIR 128
#define MAX_VIC_FIELD_LISTENER 10

#define MAX_KEY 16
#define MAX_VALUE 48

#define MAX_FIELDNAME 16

class VEDirectText;

typedef void (VEDirectText::*VicFieldListenerCallback)(DynamicJsonDocument &,
                                                       const char *, const char *);

struct VicPair
{
    VicPair();

    char key[MAX_KEY];
    char value[MAX_VALUE];
};

struct VicFieldListener
{
    VicFieldListener();

    char fieldName[MAX_FIELDNAME];
    VicFieldListenerCallback callback;
};

class VEDirectText
{
public:
    static bool loadDefs(File dataFile);
    static const char *getLoadDefsError();

public:
    VEDirectText();

    const char *getLastError();

    VicPair *findKey(const char *key);

    VicPair *findEmptyPair();

    void addFieldListener(const char *fieldName,
                          VicFieldListenerCallback callback);

    void callFieldListener(const char *fieldName,
                           DynamicJsonDocument &updates,
                           const char *fieldValue,
                           const char *unitsValue);

    void updateCurrentData(DynamicJsonDocument &updates,
                           const char *fieldKey,
                           const char *fieldValue,
                           const char *unitsKey,
                           const char *unitsValue);

    void handleLine(DynamicJsonDocument &updates, char *line);

    void formatValue(char *destValue,
                     size_t sizeValue,
                     char *destUnits,
                     size_t sizeUnits,
                     const char *value,
                     const char *vicType);

    void vpvUpdated(DynamicJsonDocument &updates,
                    const char *fieldValue,
                    const char *unitsValue);
    void ppvUpdated(DynamicJsonDocument &updates,
                    const char *fieldValue,
                    const char *unitsValue);
    void vUpdated(DynamicJsonDocument &updates,
                  const char *fieldValue,
                  const char *unitsValue);
    void iUpdated(DynamicJsonDocument &updates,
                  const char *fieldValue,
                  const char *unitsValue);
    void pUpdated(DynamicJsonDocument &updates,
                  const char *fieldValue,
                  const char *unitsValue);

    void updateIpv(DynamicJsonDocument &updates,
                   float amps);
    void updateP(DynamicJsonDocument &updates,
                 int watts);
    void updateEff(DynamicJsonDocument &updates,
                   float pct);

private:
    char _lastError[MAX_ERROR_LEN];

    VicPair _currentData[MAX_VIC_PAIR];
    VicFieldListener _fieldListeners[MAX_VIC_FIELD_LISTENER];

protected:
    static DynamicJsonDocument g_victronDefs;
    static char g_loadDefsError[MAX_ERROR_LEN];
};

#endif