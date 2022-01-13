#ifndef CONFIGPXI_H
#define CONFIGPXI_H

#include "tlmServer.h"

// Set the rotator configuration file.
void configPxi_setConfigFile(const char *pFilePath);

// Get the rotator configuration file.
char *configPxi_getConfigFile(void);

// Get the setting from file (internal use only). The user needs to free the
// the output memory if it is not needed anymore.
char *configPxi_getSetting(const char *pFilePath, const char *pSettingName);

// Get the double value in the setting.
double configPxi_getValDouble(char *pSettingName);

// Get the integer value in the setting.
int configPxi_getValInt(char *pSettingName);

// Set the server information of data distribution service (DDS).
void configPxi_setServerInfoDds(serverInfo_t *pServerInfo);

// Set the server information of graphical user interface (GUI).
void configPxi_setServerInfoGui(serverInfo_t *pServerInfo);

#endif // CONFIGPXI_H
