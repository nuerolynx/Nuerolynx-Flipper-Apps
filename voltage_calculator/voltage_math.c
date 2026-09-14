#include "voltage_math.h"

#include <string.h>

typedef struct {
    int awg;
    const char* name;
    float copper_ohms_per_1000ft_20c;
} NlxWireGauge;

/*
 * DC resistance for standard annealed copper at 20 C, rounded from the
 * NIST copper wire tables. Aluminum is calculated using the resistivity
 * ratio and the selected conductor temperature is applied afterward.
 */
static const NlxWireGauge nlx_wire_gauges[] = {
    {-2, "00", 0.07793f},
    {0, "0", 0.09827f},
    {1, "1", 0.1239f},
    {2, "2", 0.1563f},
    {3, "3", 0.1970f},
    {4, "4", 0.2485f},
    {5, "5", 0.3133f},
    {6, "6", 0.3951f},
    {7, "7", 0.4982f},
    {8, "8", 0.6282f},
    {9, "9", 0.7921f},
    {10, "10", 0.9989f},
    {11, "11", 1.260f},
    {12, "12", 1.588f},
    {13, "13", 2.003f},
    {14, "14", 2.525f},
    {15, "15", 3.184f},
    {16, "16", 4.016f},
    {17, "17", 5.064f},
    {18, "18", 6.385f},
    {19, "19", 8.051f},
    {20, "20", 10.15f},
    {21, "21", 12.80f},
    {22, "22", 16.14f},
    {23, "23", 20.36f},
    {24, "24", 25.67f},
};

static const float aluminum_to_copper_resistivity_ratio = 1.6382f;
static const float copper_temperature_coefficient = 0.00393f;
static const float aluminum_temperature_coefficient = 0.00403f;

size_t nlx_wire_gauge_count(void) {
    return sizeof(nlx_wire_gauges) / sizeof(nlx_wire_gauges[0]);
}

int nlx_wire_gauge_at(size_t index) {
    if(index >= nlx_wire_gauge_count()) return 0;
    return nlx_wire_gauges[index].awg;
}

const char* nlx_wire_gauge_name_at(size_t index) {
    if(index >= nlx_wire_gauge_count()) return "?";
    return nlx_wire_gauges[index].name;
}

bool nlx_wire_gauge_find(const char* name, size_t* index) {
    if(!name || !index) return false;
    for(size_t i = 0U; i < nlx_wire_gauge_count(); ++i) {
        if(strcmp(name, nlx_wire_gauges[i].name) == 0) {
            *index = i;
            return true;
        }
    }
    return false;
}

size_t nlx_wire_gauge_step(size_t current_index, int direction) {
    const size_t count = nlx_wire_gauge_count();
    if(current_index >= count) current_index = 0U;
    if(direction > 0) return (current_index + 1U) % count;
    if(direction < 0) return current_index == 0U ? count - 1U : current_index - 1U;
    return current_index;
}

float nlx_wire_resistance_ohms_per_1000ft(
    size_t gauge_index,
    NlxWireMaterial material,
    float conductor_temperature_c) {
    if(gauge_index >= nlx_wire_gauge_count()) return 0.0f;

    float resistance = nlx_wire_gauges[gauge_index].copper_ohms_per_1000ft_20c;
    float coefficient = copper_temperature_coefficient;
    if(material == NlxWireMaterialAluminum) {
        resistance *= aluminum_to_copper_resistivity_ratio;
        coefficient = aluminum_temperature_coefficient;
    }

    return resistance * (1.0f + coefficient * (conductor_temperature_c - 20.0f));
}

static bool nlx_recommended_gauge(
    float current_amps,
    float source_voltage,
    float one_way_length_feet,
    NlxWireMaterial material,
    float conductor_temperature_c,
    float target_drop_percent,
    size_t* gauge_index) {
    const float allowed_drop = source_voltage * target_drop_percent / 100.0f;

    /* Start with the physically smallest conductor and work upward. */
    for(size_t offset = 0; offset < nlx_wire_gauge_count(); offset++) {
        const size_t index = nlx_wire_gauge_count() - 1U - offset;
        const float resistance = nlx_wire_resistance_ohms_per_1000ft(
            index, material, conductor_temperature_c);
        const float drop = current_amps * resistance * (2.0f * one_way_length_feet) / 1000.0f;
        if(drop <= allowed_drop) {
            *gauge_index = index;
            return true;
        }
    }

    return false;
}

bool nlx_voltage_drop_calculate(
    float source_voltage,
    float load_value,
    NlxLoadMode load_mode,
    float one_way_length_feet,
    size_t gauge_index,
    NlxWireMaterial material,
    float conductor_temperature_c,
    float target_drop_percent,
    NlxVoltageDropResult* result) {
    if(!result || source_voltage <= 0.0f || load_value <= 0.0f || one_way_length_feet <= 0.0f ||
       target_drop_percent <= 0.0f || gauge_index >= nlx_wire_gauge_count()) {
        return false;
    }

    const float current_amps =
        load_mode == NlxLoadModeWatts ? load_value / source_voltage : load_value;
    if(current_amps <= 0.0f) return false;

    const float resistance_per_1000ft = nlx_wire_resistance_ohms_per_1000ft(
        gauge_index, material, conductor_temperature_c);
    const float loop_resistance =
        resistance_per_1000ft * (2.0f * one_way_length_feet) / 1000.0f;
    const float drop = current_amps * loop_resistance;
    const float drop_percent = drop * 100.0f / source_voltage;

    result->current_amps = current_amps;
    result->loop_resistance_ohms = loop_resistance;
    result->voltage_drop = drop;
    result->voltage_drop_percent = drop_percent;
    result->load_voltage = source_voltage > drop ? source_voltage - drop : 0.0f;
    result->power_loss_watts = current_amps * current_amps * loop_resistance;
    result->maximum_one_way_feet =
        (source_voltage * target_drop_percent / 100.0f) * 1000.0f /
        (2.0f * current_amps * resistance_per_1000ft);
    result->recommended_gauge_present = nlx_recommended_gauge(
        current_amps,
        source_voltage,
        one_way_length_feet,
        material,
        conductor_temperature_c,
        target_drop_percent,
        &result->recommended_gauge_index);

    if(drop_percent <= target_drop_percent) {
        result->verdict = NlxWireVerdictAdequate;
    } else if(drop_percent <= target_drop_percent * 1.15f) {
        result->verdict = NlxWireVerdictMarginal;
    } else {
        result->verdict = NlxWireVerdictUpsize;
    }

    return true;
}
