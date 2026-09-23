#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool FzeroVehiclesActive(void);
bool FzeroVehiclesLoad(const uint8_t *stock, size_t size, char *error,
                       size_t cap);
bool FzeroVehiclesPrepare(uint8_t **rom, size_t *size, char *error, size_t cap);
void FzeroVehiclesSync(void);
void FzeroVehiclesLoaded(void);
void FzeroVehiclesInstallHooks(void);
const char *FzeroVehicleIdentity(void);
unsigned FzeroVehicleCount(void);
unsigned FzeroVehicleSelected(void);
unsigned FzeroVehicleAcceleration(unsigned slot, unsigned speed);
unsigned FzeroVehicleTurn(unsigned speed);
