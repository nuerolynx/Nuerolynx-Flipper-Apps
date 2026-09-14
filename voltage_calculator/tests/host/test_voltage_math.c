#include "../../voltage_math.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_complete_gauge_sequence(void) {
    assert(nlx_wire_gauge_count() == 26U);
    assert(strcmp(nlx_wire_gauge_name_at(0U), "00") == 0);
    assert(strcmp(nlx_wire_gauge_name_at(1U), "0") == 0);

    for(size_t index = 2U; index < nlx_wire_gauge_count(); ++index) {
        char expected[4];
        snprintf(expected, sizeof(expected), "%u", (unsigned)(index - 1U));
        assert(strcmp(nlx_wire_gauge_name_at(index), expected) == 0);
    }

    size_t index = 99U;
    assert(nlx_wire_gauge_find("00", &index) && index == 0U);
    assert(nlx_wire_gauge_find("0", &index) && index == 1U);
    assert(nlx_wire_gauge_find("24", &index) && index == 25U);
    assert(!nlx_wire_gauge_find("25", &index));

    assert(nlx_wire_gauge_step(0U, 1) == 1U);
    assert(nlx_wire_gauge_step(1U, -1) == 0U);
    assert(nlx_wire_gauge_step(0U, -1) == 25U);
    assert(nlx_wire_gauge_step(25U, 1) == 0U);
}

static void test_voltage_drop_and_recommendation(void) {
    NlxVoltageDropResult result;
    assert(nlx_voltage_drop_calculate(
        12.0f,
        1.0f,
        NlxLoadModeAmps,
        100.0f,
        19U,
        NlxWireMaterialCopper,
        20.0f,
        3.0f,
        &result));

    assert(fabsf(result.voltage_drop - 1.277f) < 0.002f);
    assert(result.verdict == NlxWireVerdictUpsize);
    assert(result.recommended_gauge_present);
    assert(strcmp(nlx_wire_gauge_name_at(result.recommended_gauge_index), "12") == 0);

    assert(nlx_wire_resistance_ohms_per_1000ft(0U, NlxWireMaterialCopper, 20.0f) > 0.0f);
    assert(nlx_wire_resistance_ohms_per_1000ft(25U, NlxWireMaterialCopper, 20.0f) > 0.0f);
}

int main(void) {
    test_complete_gauge_sequence();
    test_voltage_drop_and_recommendation();
    puts("NLX voltage math host tests passed");
    return 0;
}
