#ifndef DRIVETOOL_H
#define DRIVETOOL_H

#include <ecrt.h>
#include <stdint.h>

#include "ds402.h"

// Get the fault name. The user needs to provide the buffer "pName" to get the
// fault's name at most ("len" - 1) characters (to promise the NUL terminator)
// to explain the fault "code".
void driveTool_getFaultName(uint16_t code, size_t len, char *pName);

// Get the DS402 state name.
const char *driveTool_getDs402StateName(enum ds402_state state);

// Get the U16 SDO parameter of the slave configuration "pSc" with the index and
// subindex. The user needs to provide the "pValue" to get the parameter.
// Return 0 on success, -1 on failure.
int driveTool_getSdoParamU16(ec_slave_config_t *pSc, uint16_t index,
                             uint8_t subindex, uint16_t *pValue);

// Get the U32 SDO parameter of the slave configuration "pSc" with the index and
// subindex. The user needs to provide the "pValue" to get the parameter.
// Return 0 on success, -1 on failure.
int driveTool_getSdoParamU32(ec_slave_config_t *pSc, uint16_t index,
                             uint8_t subindex, uint32_t *pValue);

// Get the S16 SDO parameter of the slave configuration "pSc" with the index and
// subindex. The user needs to provide the "pValue" to get the parameter.
// Return 0 on success, -1 on failure.
int driveTool_getSdoParamS16(ec_slave_config_t *pSc, uint16_t index,
                             uint8_t subindex, int16_t *pValue);

// Get the S32 SDO parameter of the slave configuration "pSc" with the index and
// subindex. The user needs to provide the "pValue" to get the parameter.
// Return 0 on success, -1 on failure.
int driveTool_getSdoParamS32(ec_slave_config_t *pSc, uint16_t index,
                             uint8_t subindex, int32_t *pValue);

#endif // DRIVETOOL_H
