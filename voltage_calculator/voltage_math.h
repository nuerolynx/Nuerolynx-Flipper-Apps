#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    NlxWireMaterialCopper,
    NlxWireMaterialAluminum,
} NlxWireMaterial;

typedef enum {
    NlxLoadModeAmps,
    NlxLoadModeWatts,
} NlxLoadMode;

typedef enum {
    NlxWireVerdictAdequate,
    NlxWireVerdictMarginal,
    NlxWireVerdictUpsize,
} NlxWireVerdict;

typedef struct {
    float current_amps;
    float loop_resistance_ohms;
    float voltage_drop;
    float voltage_drop_percent;
    float load_voltage;
    float power_loss_watts;
    float maximum_one_way_feet;
    bool recommended_gauge_present;
    size_t recommended_gauge_index;
    NlxWireVerdict verdict;
} NlxVoltageDropResult;

size_t nlx_wire_gauge_count(void);
int nlx_wire_gauge_at(size_t index);
const char* nlx_wire_gauge_name_at(size_t index);
bool nlx_wire_gauge_find(const char* name, size_t* index);
size_t nlx_wire_gauge_step(size_t current_index, int direction);
float nlx_wire_resistance_ohms_per_1000ft(
    size_t gauge_index,
    NlxWireMaterial material,
    float conductor_temperature_c);
bool nlx_voltage_drop_calculate(
    float source_voltage,
    float load_value,
    NlxLoadMode load_mode,
    float one_way_length_feet,
    size_t gauge_index,
    NlxWireMaterial material,
    float conductor_temperature_c,
    float target_drop_percent,
    NlxVoltageDropResult* result);
