#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "ve_direct_text.hpp"

#define CALL_MEMBER_FN(object, ptrToMember) ((object).*(ptrToMember))

//
// Static data & functions
//

DynamicJsonDocument VEDirectText::g_victronDefs(32768);
char VEDirectText::g_loadDefsError[MAX_ERROR_LEN];

bool VEDirectText::loadDefs(File dataFile)
{
    g_loadDefsError[0] = 0;
    DeserializationError error = deserializeJson(g_victronDefs, dataFile);
    if (error)
    {
        sprintf(g_loadDefsError, "VEDirectText::loadDefs: Error parsing data file '%s' [%s]", dataFile.name(), error.c_str());
        return false;
    }

    return true;
}

const char *VEDirectText::getLoadDefsError()
{
    return g_loadDefsError;
}

//
// Normal functions
//

VicPair::VicPair()
    : key(""), value("") {}

VicFieldListener::VicFieldListener()
    : fieldName("") {}

VEDirectText::VEDirectText()
    : _lastError("")
{
    addFieldListener("vpv", &VEDirectText::vpvUpdated);
    addFieldListener("ppv", &VEDirectText::ppvUpdated);
    addFieldListener("v", &VEDirectText::vUpdated);
    addFieldListener("i", &VEDirectText::iUpdated);
    addFieldListener("p", &VEDirectText::pUpdated);
}

const char *VEDirectText::getLastError()
{
    return _lastError;
}

VicPair *VEDirectText::findKey(const char *key)
{
    for (int i = 0; i < MAX_VIC_PAIR; i++)
    {
        if (strcmp(key, _currentData[i].key) == 0)
        {
            return (&(_currentData[i]));
        }
    }

    return (0);
}

VicPair *VEDirectText::findEmptyPair()
{
    for (int i = 0; i < MAX_VIC_PAIR; i++)
    {
        if (_currentData[i].key[0] == '\0')
        {
            return (&(_currentData[i]));
        }
    }

    return (0);
}

void VEDirectText::addFieldListener(const char *fieldName,
                                    VicFieldListenerCallback callback)
{
    // Find empty slot
    for (int i = 0; i < MAX_VIC_FIELD_LISTENER; i++)
    {
        if (_fieldListeners[i].fieldName[0] == '\0')
        {
            // Empty slot, set fields and return
            strcpy(_fieldListeners[i].fieldName, fieldName);
            _fieldListeners[i].callback = callback;
            return;
        }
    }
}

void VEDirectText::callFieldListener(const char *fieldName,
                                     DynamicJsonDocument &updates,
                                     const char *fieldValue,
                                     const char *unitsValue)
{
    for (int i = 0; i < MAX_VIC_FIELD_LISTENER; i++)
    {
        if (strcmp(_fieldListeners[i].fieldName, fieldName) == 0)
        {
            CALL_MEMBER_FN(*this, _fieldListeners[i].callback)
            (updates, fieldValue, unitsValue);
        }
    }
}

void VEDirectText::updateCurrentData(DynamicJsonDocument &updates,
                                     const char *fieldKey,
                                     const char *fieldValue,
                                     const char *unitsKey,
                                     const char *unitsValue)
{
    int fieldChanged = 0;
    int unitsChanged = 0;

    VicPair *fieldKeyPair = findKey(fieldKey);
    if (fieldKeyPair != 0)
    {
        if (strcmp(fieldValue, fieldKeyPair->value) != 0)
        {
            // Value for field key changed
            fieldChanged = 1;

            // Update current data and updates
            strcpy(fieldKeyPair->value, fieldValue);
            updates[(char *)fieldKey]["value"] = (char *)fieldValue;
        }
    }
    else
    {
        // Field key/value added
        fieldChanged = 1;

        // Add to current data and updates
        fieldKeyPair = findEmptyPair();
        if (fieldKeyPair != 0)
        {
            strcpy(fieldKeyPair->key, fieldKey);
            strcpy(fieldKeyPair->value, fieldValue);
        }
        updates[(char *)fieldKey]["value"] = (char *)fieldValue;
    }

    VicPair *unitsKeyPair = findKey(unitsKey);
    if (unitsKeyPair != 0)
    {
        if (strcmp(unitsValue, unitsKeyPair->value) != 0)
        {
            // Value for units key changed
            unitsChanged = 1;

            // If the field key/value aren't already in the update, add them
            if (!fieldChanged)
            {
                updates[(char *)fieldKey]["value"] = (char *)fieldValue;
            }

            // Add units key/value to currentData and updates
            strcpy(unitsKeyPair->value, unitsValue);
            updates[(char *)fieldKey]["units"] = (char *)unitsValue;
        }
    }
    else
    {
        // Units key/value added
        unitsChanged = 1;

        // If the field key/value aren't already in the update, add them
        if (!fieldChanged)
        {
            updates[(char *)fieldKey]["value"] = (char *)fieldValue;
        }

        // Add units key/value to currentData and updates
        unitsKeyPair = findEmptyPair();
        if (unitsKeyPair != 0)
        {
            strcpy(unitsKeyPair->key, unitsKey);
            strcpy(unitsKeyPair->value, unitsValue);
        }
        updates[(char *)fieldKey]["units"] = (char *)unitsValue;
    }

    // If the field changed but the units didn't, incllude the units
    // in the update anyway
    if (fieldChanged && (!unitsChanged))
    {
        updates[(char *)fieldKey]["units"] = (char *)unitsValue;
    }

    // Call field listener, if defined for this field
    if (fieldChanged || unitsChanged)
    {
        callFieldListener(fieldKey, updates, fieldValue, unitsValue);
    }
}

void VEDirectText::handleLine(DynamicJsonDocument &updates, char *line)
{
    char *field = strtok(line, "\t");
    char *value = strtok(0, "\r");

    if ((field != 0) && (value != 0))
    {
        JsonArray fieldDefs = g_victronDefs["fields"];
        const char *vicType = "??";
#define FT_LEN 100
        char formattedValue[FT_LEN];
        char formattedUnits[FT_LEN];
        // Is this next line needed?
        // snprintf(formattedValue, FT_LEN, "?(%s)", value);
        for (JsonVariant v : fieldDefs)
        {
            JsonObject fieldDef = v.as<JsonObject>();

            if (strcmp(field, fieldDef["name"]) == 0)
            {
                vicType = fieldDef["type"];
                formatValue(formattedValue, FT_LEN,
                            formattedUnits, FT_LEN,
                            value, vicType);

                char *tmp = field;
                while (*tmp != 0)
                {
                    *tmp = tolower(*tmp);
                    tmp++;
                }
                char unitsKey[50];
                sprintf(unitsKey, "%s_units", field);

                updateCurrentData(updates,
                                  field, formattedValue,
                                  unitsKey, formattedUnits);
            }
        }
    }
}

void VEDirectText::formatValue(char *destValue,
                               size_t sizeValue,
                               char *destUnits,
                               size_t sizeUnits,
                               const char *value,
                               const char *vicType)
{
    if (strcmp(vicType, "%") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        snprintf(destUnits, sizeUnits, "%%");
    }
    else if (strcmp(vicType, "0.1 %") == 0)
    {
        float tmp = (float)atoi(value);
        snprintf(destValue, sizeValue, "%.1f", tmp / 10);
        snprintf(destUnits, sizeUnits, "%%");
    }
    else if (strcmp(vicType, "0.01 V") == 0)
    {
        float tmp = (float)atoi(value);
        snprintf(destValue, sizeValue, "%.2f", tmp / 100);
        snprintf(destUnits, sizeUnits, "V");
    }
    else if (strcmp(vicType, "0.01 kWh") == 0)
    {
        float tmp = (float)atoi(value);
        if (fabs(tmp) < 100.0)
        {
            snprintf(destValue, sizeValue, "%d", (int)(tmp * 10));
            snprintf(destUnits, sizeUnits, "Wh");
        }
        else
        {
            snprintf(destValue, sizeValue, "%.2f", tmp / 100);
            snprintf(destUnits, sizeUnits, "kWh");
        }
    }
    else if (strcmp(vicType, "0.1 A") == 0)
    {
        float tmp = (float)atoi(value);
        snprintf(destValue, sizeValue, "%.1f", tmp / 10);
        snprintf(destUnits, sizeUnits, "A");
    }
    else if (strcmp(vicType, "W") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        snprintf(destUnits, sizeUnits, "W");
    }
    else if (strcmp(vicType, "count") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "deg_C") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        snprintf(destUnits, sizeUnits, "°C");
    }
    else if (strcmp(vicType, "fw") == 0)
    {
        int candidate = 0;
        int offset = 0;
        if (value[0] == 'C')
        {
            candidate = 1;
            offset = 1;
        }
        char major = value[offset];
        char minor[10];
        strcpy(minor, value + offset + 1);
        if (candidate)
        {
            snprintf(destValue, sizeValue, "%c.%s (RC)", major, minor);
            destUnits[0] = '\0';
        }
        else
        {
            snprintf(destValue, sizeValue, "%c.%s", major, minor);
            destUnits[0] = '\0';
        }
    }
    else if (strcmp(vicType, "mA") == 0)
    {
        float tmp = (float)atoi(value);
        if (fabs(tmp) < 1000)
        {
            snprintf(destValue, sizeValue, "%s", value);
            snprintf(destUnits, sizeUnits, "mA");
        }
        else
        {
            snprintf(destValue, sizeValue, "%.1f", tmp / 1000);
            snprintf(destUnits, sizeUnits, "A");
        }
    }
    else if (strcmp(vicType, "mAh") == 0)
    {
        float tmp = (float)atoi(value);
        if (fabs(tmp) < 1000)
        {
            snprintf(destValue, sizeValue, "%s", value);
            snprintf(destUnits, sizeUnits, "mAh");
        }
        else
        {
            snprintf(destValue, sizeValue, "%.2f", tmp / 1000);
            snprintf(destUnits, sizeUnits, "Ah");
        }
    }
    else if (strcmp(vicType, "mV") == 0)
    {
        float tmp = (float)atoi(value);
        if (fabs(tmp) < 1000)
        {
            snprintf(destValue, sizeValue, "%s", value);
            snprintf(destUnits, sizeUnits, "mV");
        }
        else
        {
            snprintf(destValue, sizeValue, "%.2f", tmp / 1000);
            snprintf(destUnits, sizeUnits, "V");
        }
    }
    else if (strcmp(vicType, "map_ar") == 0)
    {
        int reasons = atoi(value);
        if (reasons == 0)
        {
            snprintf(destValue, sizeValue, "No alarm");
        }
        else
        {
            JsonArray map = g_victronDefs["map_ar"];
            const char *format = "%s";
            for (JsonVariant v : map)
            {
                JsonObject mapEntry = v.as<JsonObject>();

                if ((((int)mapEntry["key"]) & reasons) != 0)
                {
                    int charsWritten = snprintf(destValue,
                                                sizeValue,
                                                format,
                                                (const char *)mapEntry["value"]);
                    destValue += charsWritten;
                    sizeValue -= charsWritten;
                    format = " | %s";
                }
            }
        }
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "map_or") == 0)
    {
        int reasons = atoi(value);
        if (reasons == 0)
        {
            snprintf(destValue, sizeValue, "Not off");
        }
        else
        {
            JsonArray map = g_victronDefs["map_or"];
            const char *format = "%s";
            for (JsonVariant v : map)
            {
                JsonObject mapEntry = v.as<JsonObject>();

                if ((((int)mapEntry["key"]) & reasons) != 0)
                {
                    int charsWritten = snprintf(destValue,
                                                sizeValue,
                                                format,
                                                (const char *)mapEntry["value"]);
                    destValue += charsWritten;
                    sizeValue -= charsWritten;
                    format = " | %s";
                }
            }
        }
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "map_cs") == 0)
    {
        int found = 0;
        int code = atoi(value);
        JsonArray map = g_victronDefs["map_cs"];
        for (JsonVariant v : map)
        {
            JsonObject mapEntry = v.as<JsonObject>();

            if (mapEntry["key"] == code)
            {
                found = 1;
                snprintf(destValue, sizeValue, "%s", (const char *)mapEntry["value"]);
                break;
            }
        }
        if (!found)
        {
            snprintf(destValue, sizeValue, "Unknown state (%s)", value);
        }
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "map_err") == 0)
    {
        int found = 0;
        int code = atoi(value);
        JsonArray map = g_victronDefs["map_err"];
        for (JsonVariant v : map)
        {
            JsonObject mapEntry = v.as<JsonObject>();

            if (mapEntry["key"] == code)
            {
                found = 1;
                snprintf(destValue, sizeValue, "%s", (const char *)mapEntry["value"]);
                break;
            }
        }
        if (!found)
        {
            snprintf(destValue, sizeValue, "Unknown error (%s)", value);
        }
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "map_mode") == 0)
    {
        int found = 0;
        int code = atoi(value);
        JsonArray map = g_victronDefs["map_mode"];
        for (JsonVariant v : map)
        {
            JsonObject mapEntry = v.as<JsonObject>();

            if (mapEntry["key"] == code)
            {
                found = 1;
                snprintf(destValue, sizeValue, "%s", (const char *)mapEntry["value"]);
                break;
            }
        }
        if (!found)
        {
            snprintf(destValue, sizeValue, "Unknown mode (%s)", value);
        }
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "map_mppt") == 0)
    {
        int found = 0;
        int code = atoi(value);
        JsonArray map = g_victronDefs["map_mppt"];
        for (JsonVariant v : map)
        {
            JsonObject mapEntry = v.as<JsonObject>();

            if (mapEntry["key"] == code)
            {
                found = 1;
                snprintf(destValue, sizeValue, "%s", (const char *)mapEntry["value"]);
                break;
            }
        }
        if (!found)
        {
            snprintf(destValue, sizeValue, "Unknown mppt (%s)", value);
        }
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "map_pid") == 0)
    {
        int found = 0;
        JsonArray map = g_victronDefs["map_pid"];
        for (JsonVariant v : map)
        {
            JsonObject mapEntry = v.as<JsonObject>();

            if (strcmp(mapEntry["key"], value) == 0)
            {
                found = 1;
                snprintf(destValue, sizeValue, "%s", (const char *)mapEntry["value"]);
                break;
            }
        }
        if (!found)
        {
            snprintf(destValue, sizeValue, "Unknown product (%s)", value);
        }
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "min") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        snprintf(destUnits, sizeUnits, "min");
    }
    else if (strcmp(vicType, "onoff") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "range[0..364]") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "sec") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        snprintf(destUnits, sizeUnits, "sec");
    }
    else if (strcmp(vicType, "serial") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        destUnits[0] = '\0';
    }
    else if (strcmp(vicType, "string") == 0)
    {
        snprintf(destValue, sizeValue, "%s", value);
        destUnits[0] = '\0';
    }
}

void VEDirectText::vpvUpdated(DynamicJsonDocument &updates,
                              const char *fieldValue,
                              const char *unitsValue)
{
    VicPair *ppv = findKey("ppv");
    if (ppv != 0)
    {
        float watts = (float)atoi(ppv->value);
        float volts = atof(fieldValue);
        if (strcmp(unitsValue, "mV") == 0)
        {
            volts /= 1000.0;
        }
        float amps = watts / volts;

        updateIpv(updates, amps);
    }
}

void VEDirectText::ppvUpdated(DynamicJsonDocument &updates,
                              const char *fieldValue,
                              const char *unitsValue)
{
    VicPair *vpv = findKey("vpv");
    VicPair *vpv_units = findKey("vpv_units");
    if ((vpv != 0) && (vpv_units != 0))
    {
        float volts = atof(vpv->value);
        if (strcmp(vpv_units->value, "mV") == 0)
        {
            volts /= 1000.0;
        }
        float watts = (float)atoi(fieldValue);
        float amps = watts / volts;

        updateIpv(updates, amps);
    }

    VicPair *p = findKey("p");
    if (p != 0)
    {
        int wattsBat = atoi(p->value);
        int wattsPv = atoi(fieldValue);
        float efficiencyPct = 0.0;
        if ((wattsPv != 0) && (wattsBat != 0))
        {
            efficiencyPct = ((float)wattsBat / (float)wattsPv) * 100.0;
        }

        updateEff(updates, efficiencyPct);
    }
}

void VEDirectText::vUpdated(DynamicJsonDocument &updates,
                            const char *fieldValue,
                            const char *unitsValue)
{
    VicPair *i = findKey("i");
    VicPair *i_units = findKey("i_units");
    if ((i != 0) && (i_units != 0))
    {
        float amps = atof(i->value);
        if (strcmp(i_units->value, "mA") == 0)
        {
            amps /= 1000.0;
        }
        float volts = atof(fieldValue);
        if (strcmp(unitsValue, "mV") == 0)
        {
            volts /= 1000.0;
        }
        float watts = volts * amps;

        updateP(updates, watts);
    }
}

void VEDirectText::iUpdated(DynamicJsonDocument &updates,
                            const char *fieldValue,
                            const char *unitsValue)
{
    VicPair *v = findKey("v");
    VicPair *v_units = findKey("v_units");
    if ((v != 0) && (v_units != 0))
    {
        float volts = atof(v->value);
        if (strcmp(v_units->value, "mV") == 0)
        {
            volts /= 1000.0;
        }
        float amps = atof(fieldValue);
        if (strcmp(unitsValue, "mA") == 0)
        {
            amps /= 1000.0;
        }
        float watts = volts * amps;

        updateP(updates, watts);
    }
}

void VEDirectText::pUpdated(DynamicJsonDocument &updates,
                            const char *fieldValue,
                            const char *unitsValue)
{
    VicPair *ppv = findKey("ppv");
    if (ppv != 0)
    {
        int wattsPv = atoi(ppv->value);
        int wattsBat = atoi(fieldValue);
        float efficiencyPct = 0.0;
        if ((wattsPv != 0) && (wattsBat != 0))
        {
            efficiencyPct = ((float)wattsBat / (float)wattsPv) * 100.0;
        }

        updateEff(updates, efficiencyPct);
    }
}

void VEDirectText::updateIpv(DynamicJsonDocument &updates,
                             float amps)
{
    const char *units = "A";
    char tmp[50];
    if (amps < 1.0)
    {
        amps *= 10.0;
        int roundAmps = roundf(amps);
        roundAmps *= 100;
        if (roundAmps == 1000)
        {
            units = "A";
            sprintf(tmp, "%.1f", 1.0);
        }
        else
        {
            units = "mA";
            sprintf(tmp, "%d", roundAmps);
        }
    }
    else
    {
        sprintf(tmp, "%.1f", amps);
    }

    updateCurrentData(updates,
                      "ipv", tmp,
                      "ipv_units", units);
}

void VEDirectText::updateP(DynamicJsonDocument &updates,
                           int watts)
{
    char tmp[50];
    sprintf(tmp, "%d", watts);

    updateCurrentData(updates,
                      "p", tmp,
                      "p_units", "W");
}

void VEDirectText::updateEff(DynamicJsonDocument &updates,
                             float pct)
{
    char tmp[50];
    sprintf(tmp, "%d", (int)roundf(pct));

    updateCurrentData(updates,
                      "eff", tmp,
                      "eff_units", "%");
}
