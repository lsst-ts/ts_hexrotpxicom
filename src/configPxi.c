#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <yaml.h>

#include "configPxi.h"
#include "utility.h"

// Configuration file path
static char *pgStrConfigFilePath = "";

void configPxi_setConfigFile(const char *pFilePath) {
    size_t length = strlen(pFilePath);
    pgStrConfigFilePath = (char *)calloc(length + 1, sizeof(char));

    // A string of length n requires n+1 bytes of storage
    strncpy(pgStrConfigFilePath, pFilePath, length + 1);
}

char *configPxi_getConfigFile(void) { return pgStrConfigFilePath; }

char *configPxi_getSetting(const char *pFilePath, const char *pSettingName) {

    char *pStrSettingValue = "";
    FILE *fp = fopen(pFilePath, "r");

    // Initialize parser
    yaml_parser_t parser;
    if (yaml_parser_initialize(&parser) == 0) {
        perror("Failed to initialize parser!\n");
    }

    if (fp != NULL) {
        yaml_parser_set_input_file(&parser, fp);

        yaml_event_t event;
        yaml_event_type_t type;
        yaml_char_t *value;
        size_t valueSize;
        bool foundKey = false;
        while (true) {

            if (yaml_parser_parse(&parser, &event) == 0) {
                syslog(LOG_ERR, "Parser error: %d", parser.error);
                exit(EXIT_FAILURE);
            }
            type = event.type;

            if (type == YAML_SCALAR_EVENT) {

                // Look for the key
                value = event.data.scalar.value;
                valueSize = event.data.scalar.length;
                if (strncmp((char *)value, pSettingName, valueSize) == 0) {
                    foundKey = true;

                    // The value of key will be in the next YAML_SCALAR_EVENT
                    continue;
                }

                // Copy the value of key
                // Allocate the size (= valueSize + 1) here because there is no
                // idea that the valueSize considers the '\0' or not
                pStrSettingValue = (char *)calloc(valueSize + 1, sizeof(char));
                if (foundKey) {
                    strncpy(pStrSettingValue, (char *)value, valueSize + 1);
                }
            }

            yaml_event_delete(&event);

            if (type == YAML_STREAM_END_EVENT || foundKey) {
                break;
            }
        }
    } else {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }

    yaml_parser_delete(&parser);
    fclose(fp);

    return pStrSettingValue;
}

double configPxi_getValDouble(char *pSettingName) {
    char *pStrSetting = configPxi_getSetting(pgStrConfigFilePath, pSettingName);
    double val = atof(pStrSetting);

    free(pStrSetting);
    pStrSetting = NULL;

    return val;
}

int configPxi_getValInt(char *pSettingName) {
    char *pStrSetting = configPxi_getSetting(pgStrConfigFilePath, pSettingName);
    int val = atoi(pStrSetting);

    free(pStrSetting);
    pStrSetting = NULL;

    return val;
}
