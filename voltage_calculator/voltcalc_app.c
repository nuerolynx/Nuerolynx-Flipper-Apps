#include <furi.h>

#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/scene_manager.h>
#include <gui/view_stack.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "voltage_math.h"

#define TAG "NlxElectrical"
#define INPUT_BUFFER_SIZE 32
#define LABEL_COUNT 10
#define LABEL_SIZE 40

typedef enum {
    NlxSceneMain,
    NlxSceneDropConfig,
    NlxSceneDropResult,
    NlxSceneOhmConfig,
    NlxSceneOhmResult,
    NlxSceneTextInput,
    NlxSceneAbout,
    NlxSceneCount,
} NlxScene;

typedef enum {
    NlxViewSubmenu,
    NlxViewWidget,
    NlxViewTextInput,
} NlxView;

typedef enum {
    NlxMainActionVoltageDrop,
    NlxMainActionOhmsLaw,
    NlxMainActionAbout,
} NlxMainAction;

typedef enum {
    NlxDropActionSource,
    NlxDropActionLoadMode,
    NlxDropActionLoad,
    NlxDropActionLength,
    NlxDropActionUnits,
    NlxDropActionGauge,
    NlxDropActionMaterial,
    NlxDropActionTemperature,
    NlxDropActionTarget,
    NlxDropActionCalculate,
} NlxDropAction;

typedef enum {
    NlxOhmActionMode,
    NlxOhmActionInputA,
    NlxOhmActionInputB,
    NlxOhmActionCalculate,
} NlxOhmAction;

typedef enum {
    NlxEditNone,
    NlxEditSourceVoltage,
    NlxEditLoad,
    NlxEditLength,
    NlxEditTemperature,
    NlxEditTargetDrop,
    NlxEditWireGauge,
    NlxEditOhmInputA,
    NlxEditOhmInputB,
} NlxEditField;

typedef enum {
    NlxLengthFeet,
    NlxLengthMeters,
} NlxLengthUnit;

typedef enum {
    NlxOhmSolveVoltage,
    NlxOhmSolveCurrent,
    NlxOhmSolveResistance,
} NlxOhmSolveMode;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    Submenu* submenu;
    ViewStack* submenu_stack;
    View* submenu_input_view;
    Widget* widget;
    TextInput* text_input;

    char input_buffer[INPUT_BUFFER_SIZE];
    char labels[LABEL_COUNT][LABEL_SIZE];
    NlxEditField edit_field;

    float source_voltage;
    float load_value;
    float one_way_length;
    float conductor_temperature_c;
    float target_drop_percent;
    size_t gauge_index;
    NlxWireMaterial material;
    NlxLoadMode load_mode;
    NlxLengthUnit length_unit;
    NlxVoltageDropResult drop_result;

    NlxOhmSolveMode ohm_mode;
    float ohm_input_a;
    float ohm_input_b;
    float ohm_voltage;
    float ohm_current;
    float ohm_resistance;
    float ohm_power;
} NlxElectricalApp;

enum {
    NlxEventTextSaved = 100,
    NlxEventExactEditBase = 200,
};

static void nlx_submenu_callback(void* context, uint32_t index) {
    NlxElectricalApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void nlx_numeric_submenu_callback(void* context, InputType input_type, uint32_t index) {
    NlxElectricalApp* app = context;
    if(input_type == InputTypeShort || input_type == InputTypeLong) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NlxEventExactEditBase + index);
    }
}

static void nlx_text_saved_callback(void* context) {
    NlxElectricalApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NlxEventTextSaved);
}

static const char* nlx_material_name(NlxWireMaterial material) {
    return material == NlxWireMaterialCopper ? "Copper" : "Aluminum";
}

static const char* nlx_verdict_name(NlxWireVerdict verdict) {
    switch(verdict) {
    case NlxWireVerdictAdequate:
        return "ADEQUATE";
    case NlxWireVerdictMarginal:
        return "MARGINAL";
    case NlxWireVerdictUpsize:
        return "UPSIZE REQUIRED";
    default:
        return "UNKNOWN";
    }
}

static void nlx_format_value(char* buffer, size_t size, float value) {
    snprintf(buffer, size, "%.4g", (double)value);
    for(char* cursor = buffer; *cursor; cursor++) {
        if(*cursor == '.') *cursor = '_';
    }
}

static bool nlx_parse_value(const char* input, float minimum, float maximum, float* value) {
    char normalized[INPUT_BUFFER_SIZE];
    strlcpy(normalized, input, sizeof(normalized));
    for(char* cursor = normalized; *cursor; cursor++) {
        if(*cursor == '_') *cursor = '.';
    }

    char* end = NULL;
    const float parsed = strtof(normalized, &end);
    if(end == normalized || *end != '\0' || parsed < minimum || parsed > maximum) return false;
    *value = parsed;
    return true;
}

static const char* nlx_edit_header(NlxEditField field) {
    switch(field) {
    case NlxEditSourceVoltage:
        return "Source voltage (V)";
    case NlxEditLoad:
        return "Load value (_ = .)";
    case NlxEditLength:
        return "One-way length";
    case NlxEditTemperature:
        return "Wire temp C";
    case NlxEditTargetDrop:
        return "Allowed drop %";
    case NlxEditWireGauge:
        return "Wire gauge (00, 0-24)";
    case NlxEditOhmInputA:
        return "First value (_ = .)";
    case NlxEditOhmInputB:
        return "Second value (_ = .)";
    default:
        return "Enter value";
    }
}

static float nlx_edit_current_value(NlxElectricalApp* app, NlxEditField field) {
    switch(field) {
    case NlxEditSourceVoltage:
        return app->source_voltage;
    case NlxEditLoad:
        return app->load_value;
    case NlxEditLength:
        return app->one_way_length;
    case NlxEditTemperature:
        return app->conductor_temperature_c;
    case NlxEditTargetDrop:
        return app->target_drop_percent;
    case NlxEditOhmInputA:
        return app->ohm_input_a;
    case NlxEditOhmInputB:
        return app->ohm_input_b;
    default:
        return 0.0f;
    }
}

static bool nlx_edit_bounds(NlxEditField field, float* minimum, float* maximum) {
    switch(field) {
    case NlxEditSourceVoltage:
        *minimum = 0.1f;
        *maximum = 1000.0f;
        break;
    case NlxEditLoad:
        *minimum = 0.001f;
        *maximum = 10000.0f;
        break;
    case NlxEditLength:
        *minimum = 0.1f;
        *maximum = 100000.0f;
        break;
    case NlxEditTemperature:
        *minimum = 0.0f;
        *maximum = 150.0f;
        break;
    case NlxEditTargetDrop:
        *minimum = 0.1f;
        *maximum = 25.0f;
        break;
    case NlxEditOhmInputA:
    case NlxEditOhmInputB:
        *minimum = 0.0001f;
        *maximum = 1000000.0f;
        break;
    default:
        return false;
    }
    return true;
}

static void nlx_set_edit_value(NlxElectricalApp* app, NlxEditField field, float value) {
    switch(field) {
    case NlxEditSourceVoltage:
        app->source_voltage = value;
        break;
    case NlxEditLoad:
        app->load_value = value;
        break;
    case NlxEditLength:
        app->one_way_length = value;
        break;
    case NlxEditTemperature:
        app->conductor_temperature_c = value;
        break;
    case NlxEditTargetDrop:
        app->target_drop_percent = value;
        break;
    case NlxEditOhmInputA:
        app->ohm_input_a = value;
        break;
    case NlxEditOhmInputB:
        app->ohm_input_b = value;
        break;
    default:
        break;
    }
}

static void nlx_begin_edit(NlxElectricalApp* app, NlxEditField field) {
    app->edit_field = field;
    if(field == NlxEditWireGauge) {
        strlcpy(
            app->input_buffer,
            nlx_wire_gauge_name_at(app->gauge_index),
            sizeof(app->input_buffer));
    } else {
        nlx_format_value(
            app->input_buffer, sizeof(app->input_buffer), nlx_edit_current_value(app, field));
    }
    scene_manager_next_scene(app->scene_manager, NlxSceneTextInput);
}

static void nlx_apply_edit(NlxElectricalApp* app) {
    float value = 0.0f;
    bool valid = false;

    switch(app->edit_field) {
    case NlxEditSourceVoltage:
        valid = nlx_parse_value(app->input_buffer, 0.1f, 1000.0f, &value);
        if(valid) app->source_voltage = value;
        break;
    case NlxEditLoad:
        valid = nlx_parse_value(app->input_buffer, 0.001f, 10000.0f, &value);
        if(valid) app->load_value = value;
        break;
    case NlxEditLength:
        valid = nlx_parse_value(app->input_buffer, 0.1f, 100000.0f, &value);
        if(valid) app->one_way_length = value;
        break;
    case NlxEditTemperature:
        valid = nlx_parse_value(app->input_buffer, 0.0f, 150.0f, &value);
        if(valid) app->conductor_temperature_c = value;
        break;
    case NlxEditTargetDrop:
        valid = nlx_parse_value(app->input_buffer, 0.1f, 25.0f, &value);
        if(valid) app->target_drop_percent = value;
        break;
    case NlxEditWireGauge:
        valid = nlx_wire_gauge_find(app->input_buffer, &app->gauge_index);
        break;
    case NlxEditOhmInputA:
        valid = nlx_parse_value(app->input_buffer, 0.0001f, 1000000.0f, &value);
        if(valid) app->ohm_input_a = value;
        break;
    case NlxEditOhmInputB:
        valid = nlx_parse_value(app->input_buffer, 0.0001f, 1000000.0f, &value);
        if(valid) app->ohm_input_b = value;
        break;
    default:
        break;
    }

    if(!valid) FURI_LOG_W(TAG, "Ignored invalid numeric input");
}

static void nlx_render_main(NlxElectricalApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "NLX Electrical");
    submenu_add_item(
        app->submenu,
        "Voltage Drop / Wire Size",
        NlxMainActionVoltageDrop,
        nlx_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "Ohm's Law", NlxMainActionOhmsLaw, nlx_submenu_callback, app);
    submenu_add_item(app->submenu, "About", NlxMainActionAbout, nlx_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NlxViewSubmenu);
}

static void nlx_update_drop_labels(NlxElectricalApp* app) {
    snprintf(app->labels[0], LABEL_SIZE, "Source: %.4g V", (double)app->source_voltage);
    snprintf(
        app->labels[1],
        LABEL_SIZE,
        "Load input: %s",
        app->load_mode == NlxLoadModeAmps ? "Amps" : "Watts");
    snprintf(
        app->labels[2],
        LABEL_SIZE,
        "Load: %.4g %s",
        (double)app->load_value,
        app->load_mode == NlxLoadModeAmps ? "A" : "W");
    snprintf(
        app->labels[3],
        LABEL_SIZE,
        "One-way: %.4g %s",
        (double)app->one_way_length,
        app->length_unit == NlxLengthFeet ? "ft" : "m");
    snprintf(
        app->labels[4],
        LABEL_SIZE,
        "Units: %s",
        app->length_unit == NlxLengthFeet ? "Feet" : "Meters");
    snprintf(
        app->labels[5],
        LABEL_SIZE,
        "Wire: %s AWG",
        nlx_wire_gauge_name_at(app->gauge_index));
    snprintf(app->labels[6], LABEL_SIZE, "Material: %s", nlx_material_name(app->material));
    snprintf(
        app->labels[7],
        LABEL_SIZE,
        "Wire temp: %.4g C",
        (double)app->conductor_temperature_c);
    snprintf(
        app->labels[8], LABEL_SIZE, "Max drop: %.4g%%", (double)app->target_drop_percent);
    strlcpy(app->labels[9], "Calculate / Size Wire", LABEL_SIZE);
}

static void nlx_refresh_drop_labels(NlxElectricalApp* app) {
    nlx_update_drop_labels(app);
    for(uint32_t index = 0U; index < LABEL_COUNT; ++index) {
        submenu_change_item_label(app->submenu, index, app->labels[index]);
    }
}

static void nlx_render_drop_config(NlxElectricalApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Voltage Drop");
    nlx_update_drop_labels(app);

    for(uint32_t index = 0; index < LABEL_COUNT; index++) {
        const bool numeric = index == NlxDropActionSource || index == NlxDropActionLoad ||
                             index == NlxDropActionLength ||
                             index == NlxDropActionTemperature || index == NlxDropActionTarget ||
                             index == NlxDropActionGauge;
        if(numeric) {
            submenu_add_item_ex(
                app->submenu, app->labels[index], index, nlx_numeric_submenu_callback, app);
        } else {
            submenu_add_item(app->submenu, app->labels[index], index, nlx_submenu_callback, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, NlxViewSubmenu);
}

static void nlx_render_drop_result(NlxElectricalApp* app) {
    widget_reset(app->widget);
    FuriString* text = furi_string_alloc();
    const float length_feet = app->length_unit == NlxLengthFeet ?
                                  app->one_way_length :
                                  app->one_way_length * 3.280839895f;
    const char* recommendation = app->drop_result.recommended_gauge_present ?
                                     "See value below" :
                                     "Larger than 00 AWG";

    furi_string_printf(
        text,
        "\e#NLX Voltage Drop\n"
        "%s AWG %s @ %.0f C\n"
        "Run: %.2f ft one-way\n"
        "Current: %.3f A\n"
        "Loop R: %.4f ohm\n"
        "Drop: %.3f V (%.2f%%)\n"
        "Load voltage: %.3f V\n"
        "Wire loss: %.3f W\n"
        "Target: %.2f%%\n"
        "\e#%s\n"
        "Minimum gauge: %s",
        nlx_wire_gauge_name_at(app->gauge_index),
        nlx_material_name(app->material),
        (double)app->conductor_temperature_c,
        (double)length_feet,
        (double)app->drop_result.current_amps,
        (double)app->drop_result.loop_resistance_ohms,
        (double)app->drop_result.voltage_drop,
        (double)app->drop_result.voltage_drop_percent,
        (double)app->drop_result.load_voltage,
        (double)app->drop_result.power_loss_watts,
        (double)app->target_drop_percent,
        nlx_verdict_name(app->drop_result.verdict),
        recommendation);

    if(app->drop_result.recommended_gauge_present) {
        furi_string_cat_printf(
            text,
            "\n%s AWG",
            nlx_wire_gauge_name_at(app->drop_result.recommended_gauge_index));
    }
    furi_string_cat_printf(
        text,
        "\nMax run at target: %.1f ft"
        "\n\nSizing verdict is based on voltage drop. Verify cable ampacity, listing, "
        "temperature rating, terminals, and local code.",
        (double)app->drop_result.maximum_one_way_feet);

    widget_add_text_scroll_element(app->widget, 1, 1, 126, 62, furi_string_get_cstr(text));
    furi_string_free(text);
    view_dispatcher_switch_to_view(app->view_dispatcher, NlxViewWidget);
}

static const char* nlx_ohm_mode_name(NlxOhmSolveMode mode) {
    switch(mode) {
    case NlxOhmSolveVoltage:
        return "Voltage";
    case NlxOhmSolveCurrent:
        return "Current";
    case NlxOhmSolveResistance:
        return "Resistance";
    default:
        return "Unknown";
    }
}

static void nlx_update_ohm_labels(NlxElectricalApp* app) {
    snprintf(app->labels[0], LABEL_SIZE, "Solve: %s", nlx_ohm_mode_name(app->ohm_mode));

    if(app->ohm_mode == NlxOhmSolveVoltage) {
        snprintf(app->labels[1], LABEL_SIZE, "Current: %.4g A", (double)app->ohm_input_a);
        snprintf(
            app->labels[2], LABEL_SIZE, "Resistance: %.4g ohm", (double)app->ohm_input_b);
    } else if(app->ohm_mode == NlxOhmSolveCurrent) {
        snprintf(app->labels[1], LABEL_SIZE, "Voltage: %.4g V", (double)app->ohm_input_a);
        snprintf(
            app->labels[2], LABEL_SIZE, "Resistance: %.4g ohm", (double)app->ohm_input_b);
    } else {
        snprintf(app->labels[1], LABEL_SIZE, "Voltage: %.4g V", (double)app->ohm_input_a);
        snprintf(app->labels[2], LABEL_SIZE, "Current: %.4g A", (double)app->ohm_input_b);
    }
    strlcpy(app->labels[3], "Calculate", LABEL_SIZE);
}

static void nlx_refresh_ohm_labels(NlxElectricalApp* app) {
    nlx_update_ohm_labels(app);
    for(uint32_t index = 0U; index < 4U; ++index) {
        submenu_change_item_label(app->submenu, index, app->labels[index]);
    }
}

static void nlx_render_ohm_config(NlxElectricalApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Ohm's Law");
    nlx_update_ohm_labels(app);

    for(uint32_t index = 0; index < 4; index++) {
        if(index == NlxOhmActionInputA || index == NlxOhmActionInputB) {
            submenu_add_item_ex(
                app->submenu, app->labels[index], index, nlx_numeric_submenu_callback, app);
        } else {
            submenu_add_item(app->submenu, app->labels[index], index, nlx_submenu_callback, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, NlxViewSubmenu);
}

static bool nlx_menu_adjust_numeric(
    NlxElectricalApp* app,
    NlxEditField field,
    int direction,
    InputType input_type) {
    float minimum = 0.0f;
    float maximum = 0.0f;
    if(!nlx_edit_bounds(field, &minimum, &maximum)) return false;

    const float step = input_type == InputTypeShort ? 0.1f : 1.0f;
    float value = nlx_edit_current_value(app, field) + step * (float)direction;
    if(value < minimum) value = minimum;
    if(value > maximum) value = maximum;
    nlx_set_edit_value(app, field, value);
    return true;
}

static NlxEditField nlx_drop_edit_field(uint32_t index) {
    switch(index) {
    case NlxDropActionSource:
        return NlxEditSourceVoltage;
    case NlxDropActionLoad:
        return NlxEditLoad;
    case NlxDropActionLength:
        return NlxEditLength;
    case NlxDropActionTemperature:
        return NlxEditTemperature;
    case NlxDropActionTarget:
        return NlxEditTargetDrop;
    default:
        return NlxEditNone;
    }
}

static bool nlx_drop_menu_adjust(
    NlxElectricalApp* app,
    uint32_t index,
    int direction,
    InputType input_type) {
    const NlxEditField field = nlx_drop_edit_field(index);
    if(field != NlxEditNone) {
        if(!nlx_menu_adjust_numeric(app, field, direction, input_type)) return false;
    } else {
        switch(index) {
        case NlxDropActionLoadMode:
            if(app->load_mode == NlxLoadModeAmps) {
                app->load_value *= app->source_voltage;
                app->load_mode = NlxLoadModeWatts;
            } else {
                app->load_value /= app->source_voltage;
                app->load_mode = NlxLoadModeAmps;
            }
            break;
        case NlxDropActionUnits:
            if(app->length_unit == NlxLengthFeet) {
                app->one_way_length /= 3.280839895f;
                app->length_unit = NlxLengthMeters;
            } else {
                app->one_way_length *= 3.280839895f;
                app->length_unit = NlxLengthFeet;
            }
            break;
        case NlxDropActionGauge:
            app->gauge_index = nlx_wire_gauge_step(app->gauge_index, direction);
            break;
        case NlxDropActionMaterial:
            app->material = app->material == NlxWireMaterialCopper ?
                                NlxWireMaterialAluminum :
                                NlxWireMaterialCopper;
            break;
        default:
            return false;
        }
    }

    nlx_refresh_drop_labels(app);
    return true;
}

static bool nlx_ohm_menu_adjust(
    NlxElectricalApp* app,
    uint32_t index,
    int direction,
    InputType input_type) {
    if(index == NlxOhmActionMode) {
        const int mode_count = 3;
        int mode = (int)app->ohm_mode + direction;
        if(mode < 0) mode = mode_count - 1;
        if(mode >= mode_count) mode = 0;
        app->ohm_mode = (NlxOhmSolveMode)mode;
    } else if(index == NlxOhmActionInputA || index == NlxOhmActionInputB) {
        const NlxEditField field = index == NlxOhmActionInputA ?
                                       NlxEditOhmInputA :
                                       NlxEditOhmInputB;
        if(!nlx_menu_adjust_numeric(app, field, direction, input_type)) return false;
    } else {
        return false;
    }

    nlx_refresh_ohm_labels(app);
    return true;
}

static bool nlx_submenu_input_overlay(InputEvent* event, void* context) {
    NlxElectricalApp* app = context;
    if(event->key != InputKeyLeft && event->key != InputKeyRight) return false;
    if(event->type != InputTypeShort && event->type != InputTypeLong &&
       event->type != InputTypeRepeat) {
        return false;
    }

    const int direction = event->key == InputKeyRight ? 1 : -1;
    const uint32_t selected = submenu_get_selected_item(app->submenu);
    const uint32_t scene = scene_manager_get_current_scene(app->scene_manager);
    if(scene == NlxSceneDropConfig) {
        return nlx_drop_menu_adjust(app, selected, direction, event->type);
    }
    if(scene == NlxSceneOhmConfig) {
        return nlx_ohm_menu_adjust(app, selected, direction, event->type);
    }
    return false;
}

static void nlx_calculate_ohm(NlxElectricalApp* app) {
    if(app->ohm_mode == NlxOhmSolveVoltage) {
        app->ohm_current = app->ohm_input_a;
        app->ohm_resistance = app->ohm_input_b;
        app->ohm_voltage = app->ohm_current * app->ohm_resistance;
    } else if(app->ohm_mode == NlxOhmSolveCurrent) {
        app->ohm_voltage = app->ohm_input_a;
        app->ohm_resistance = app->ohm_input_b;
        app->ohm_current = app->ohm_voltage / app->ohm_resistance;
    } else {
        app->ohm_voltage = app->ohm_input_a;
        app->ohm_current = app->ohm_input_b;
        app->ohm_resistance = app->ohm_voltage / app->ohm_current;
    }
    app->ohm_power = app->ohm_voltage * app->ohm_current;
}

static void nlx_render_ohm_result(NlxElectricalApp* app) {
    widget_reset(app->widget);
    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "\e#NLX Ohm's Law\n\n"
        "Voltage: %.5g V\n"
        "Current: %.5g A\n"
        "Resistance: %.5g ohm\n"
        "Power: %.5g W\n\n"
        "V = I x R\nP = V x I",
        (double)app->ohm_voltage,
        (double)app->ohm_current,
        (double)app->ohm_resistance,
        (double)app->ohm_power);
    widget_add_text_scroll_element(app->widget, 1, 1, 126, 62, furi_string_get_cstr(text));
    furi_string_free(text);
    view_dispatcher_switch_to_view(app->view_dispatcher, NlxViewWidget);
}

static void nlx_render_about(NlxElectricalApp* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        1,
        1,
        126,
        62,
        "\e#NLX Electrical\n\n"
        "Ohm's-law and two-conductor voltage-drop calculator for field estimates.\n\n"
        "Length is one-way; resistance includes the outbound and return conductors. "
        "Highlight a numeric field and tap Left/Right for 0.1 or hold for 1.0. Press "
        "OK for the exact keypad; use '_' as the decimal point. On Wire, Left/Right "
        "cycles through 00, 0, and 1-24 AWG.\n\n"
        "The adequacy verdict evaluates voltage drop only. It is not an ampacity, "
        "code-compliance, fault-current, or life-safety determination. Verify the "
        "actual cable datasheet and applicable requirements.\n\n"
        "Copper resistance is based on NIST standard annealed-copper data at 20 C, "
        "with a temperature correction. Aluminum values are engineering estimates.\n\n"
        "Original VoltCalc: Andrew Diamond\nNuerolynx extension: NLX");
    view_dispatcher_switch_to_view(app->view_dispatcher, NlxViewWidget);
}

static void nlx_scene_main_on_enter(void* context) {
    nlx_render_main(context);
}

static bool nlx_scene_main_on_event(void* context, SceneManagerEvent event) {
    NlxElectricalApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == NlxMainActionVoltageDrop) {
        scene_manager_next_scene(app->scene_manager, NlxSceneDropConfig);
    } else if(event.event == NlxMainActionOhmsLaw) {
        scene_manager_next_scene(app->scene_manager, NlxSceneOhmConfig);
    } else if(event.event == NlxMainActionAbout) {
        scene_manager_next_scene(app->scene_manager, NlxSceneAbout);
    } else {
        return false;
    }
    return true;
}

static void nlx_scene_main_on_exit(void* context) {
    NlxElectricalApp* app = context;
    submenu_reset(app->submenu);
}

static void nlx_scene_drop_config_on_enter(void* context) {
    nlx_render_drop_config(context);
}

static bool nlx_scene_drop_config_on_event(void* context, SceneManagerEvent event) {
    NlxElectricalApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event >= NlxEventExactEditBase &&
       event.event < NlxEventExactEditBase + LABEL_COUNT) {
        switch(event.event - NlxEventExactEditBase) {
        case NlxDropActionSource:
            nlx_begin_edit(app, NlxEditSourceVoltage);
            return true;
        case NlxDropActionLoad:
            nlx_begin_edit(app, NlxEditLoad);
            return true;
        case NlxDropActionLength:
            nlx_begin_edit(app, NlxEditLength);
            return true;
        case NlxDropActionTemperature:
            nlx_begin_edit(app, NlxEditTemperature);
            return true;
        case NlxDropActionTarget:
            nlx_begin_edit(app, NlxEditTargetDrop);
            return true;
        case NlxDropActionGauge:
            nlx_begin_edit(app, NlxEditWireGauge);
            return true;
        default:
            return false;
        }
    }

    switch(event.event) {
    case NlxDropActionSource:
        nlx_begin_edit(app, NlxEditSourceVoltage);
        break;
    case NlxDropActionLoadMode:
        if(app->load_mode == NlxLoadModeAmps) {
            app->load_value *= app->source_voltage;
            app->load_mode = NlxLoadModeWatts;
        } else {
            app->load_value /= app->source_voltage;
            app->load_mode = NlxLoadModeAmps;
        }
        nlx_render_drop_config(app);
        break;
    case NlxDropActionLoad:
        nlx_begin_edit(app, NlxEditLoad);
        break;
    case NlxDropActionLength:
        nlx_begin_edit(app, NlxEditLength);
        break;
    case NlxDropActionUnits:
        if(app->length_unit == NlxLengthFeet) {
            app->one_way_length /= 3.280839895f;
            app->length_unit = NlxLengthMeters;
        } else {
            app->one_way_length *= 3.280839895f;
            app->length_unit = NlxLengthFeet;
        }
        nlx_render_drop_config(app);
        break;
    case NlxDropActionGauge:
        nlx_begin_edit(app, NlxEditWireGauge);
        break;
    case NlxDropActionMaterial:
        app->material = app->material == NlxWireMaterialCopper ?
                            NlxWireMaterialAluminum :
                            NlxWireMaterialCopper;
        nlx_render_drop_config(app);
        break;
    case NlxDropActionTemperature:
        nlx_begin_edit(app, NlxEditTemperature);
        break;
    case NlxDropActionTarget:
        nlx_begin_edit(app, NlxEditTargetDrop);
        break;
    case NlxDropActionCalculate: {
        const float length_feet = app->length_unit == NlxLengthFeet ?
                                      app->one_way_length :
                                      app->one_way_length * 3.280839895f;
        if(nlx_voltage_drop_calculate(
               app->source_voltage,
               app->load_value,
               app->load_mode,
               length_feet,
               app->gauge_index,
               app->material,
               app->conductor_temperature_c,
               app->target_drop_percent,
               &app->drop_result)) {
            scene_manager_next_scene(app->scene_manager, NlxSceneDropResult);
        }
        break;
    }
    default:
        return false;
    }
    return true;
}

static void nlx_scene_drop_config_on_exit(void* context) {
    NlxElectricalApp* app = context;
    submenu_reset(app->submenu);
}

static void nlx_scene_drop_result_on_enter(void* context) {
    nlx_render_drop_result(context);
}

static bool nlx_scene_ignore_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

static void nlx_scene_widget_on_exit(void* context) {
    NlxElectricalApp* app = context;
    widget_reset(app->widget);
}

static void nlx_scene_ohm_config_on_enter(void* context) {
    nlx_render_ohm_config(context);
}

static bool nlx_scene_ohm_config_on_event(void* context, SceneManagerEvent event) {
    NlxElectricalApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event >= NlxEventExactEditBase && event.event < NlxEventExactEditBase + 4U) {
        switch(event.event - NlxEventExactEditBase) {
        case NlxOhmActionInputA:
            nlx_begin_edit(app, NlxEditOhmInputA);
            return true;
        case NlxOhmActionInputB:
            nlx_begin_edit(app, NlxEditOhmInputB);
            return true;
        default:
            return false;
        }
    }

    switch(event.event) {
    case NlxOhmActionMode:
        app->ohm_mode = (NlxOhmSolveMode)((app->ohm_mode + 1) % 3);
        nlx_render_ohm_config(app);
        break;
    case NlxOhmActionInputA:
        nlx_begin_edit(app, NlxEditOhmInputA);
        break;
    case NlxOhmActionInputB:
        nlx_begin_edit(app, NlxEditOhmInputB);
        break;
    case NlxOhmActionCalculate:
        nlx_calculate_ohm(app);
        scene_manager_next_scene(app->scene_manager, NlxSceneOhmResult);
        break;
    default:
        return false;
    }
    return true;
}

static void nlx_scene_ohm_config_on_exit(void* context) {
    NlxElectricalApp* app = context;
    submenu_reset(app->submenu);
}

static void nlx_scene_ohm_result_on_enter(void* context) {
    nlx_render_ohm_result(context);
}

static void nlx_scene_text_input_on_enter(void* context) {
    NlxElectricalApp* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, nlx_edit_header(app->edit_field));
    text_input_set_result_callback(
        app->text_input,
        nlx_text_saved_callback,
        app,
        app->input_buffer,
        sizeof(app->input_buffer),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, NlxViewTextInput);
}

static bool nlx_scene_text_input_on_event(void* context, SceneManagerEvent event) {
    NlxElectricalApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == NlxEventTextSaved) {
        nlx_apply_edit(app);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

static void nlx_scene_text_input_on_exit(void* context) {
    NlxElectricalApp* app = context;
    text_input_reset(app->text_input);
    app->edit_field = NlxEditNone;
}

static void nlx_scene_about_on_enter(void* context) {
    nlx_render_about(context);
}

static void (*const nlx_scene_on_enter_handlers[])(void*) = {
    nlx_scene_main_on_enter,
    nlx_scene_drop_config_on_enter,
    nlx_scene_drop_result_on_enter,
    nlx_scene_ohm_config_on_enter,
    nlx_scene_ohm_result_on_enter,
    nlx_scene_text_input_on_enter,
    nlx_scene_about_on_enter,
};

static bool (*const nlx_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    nlx_scene_main_on_event,
    nlx_scene_drop_config_on_event,
    nlx_scene_ignore_event,
    nlx_scene_ohm_config_on_event,
    nlx_scene_ignore_event,
    nlx_scene_text_input_on_event,
    nlx_scene_ignore_event,
};

static void (*const nlx_scene_on_exit_handlers[])(void*) = {
    nlx_scene_main_on_exit,
    nlx_scene_drop_config_on_exit,
    nlx_scene_widget_on_exit,
    nlx_scene_ohm_config_on_exit,
    nlx_scene_widget_on_exit,
    nlx_scene_text_input_on_exit,
    nlx_scene_widget_on_exit,
};

static const SceneManagerHandlers nlx_scene_manager_handlers = {
    .on_enter_handlers = nlx_scene_on_enter_handlers,
    .on_event_handlers = nlx_scene_on_event_handlers,
    .on_exit_handlers = nlx_scene_on_exit_handlers,
    .scene_num = NlxSceneCount,
};

static bool nlx_custom_event_callback(void* context, uint32_t event) {
    NlxElectricalApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nlx_navigation_event_callback(void* context) {
    NlxElectricalApp* app = context;
    if(!scene_manager_handle_back_event(app->scene_manager)) {
        view_dispatcher_stop(app->view_dispatcher);
    }
    return true;
}

static NlxElectricalApp* nlx_app_alloc(void) {
    NlxElectricalApp* app = malloc(sizeof(NlxElectricalApp));
    app->scene_manager = scene_manager_alloc(&nlx_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->submenu_stack = view_stack_alloc();
    app->submenu_input_view = view_alloc();
    app->widget = widget_alloc();
    app->text_input = text_input_alloc();
    app->gui = furi_record_open(RECORD_GUI);

    app->source_voltage = 12.0f;
    app->load_value = 1.0f;
    app->one_way_length = 100.0f;
    app->conductor_temperature_c = 20.0f;
    app->target_drop_percent = 3.0f;
    app->gauge_index = 19U;
    app->material = NlxWireMaterialCopper;
    app->load_mode = NlxLoadModeAmps;
    app->length_unit = NlxLengthFeet;
    app->edit_field = NlxEditNone;
    app->ohm_mode = NlxOhmSolveVoltage;
    app->ohm_input_a = 1.0f;
    app->ohm_input_b = 10.0f;

    view_set_context(app->submenu_input_view, app);
    view_set_input_callback(app->submenu_input_view, nlx_submenu_input_overlay);
    view_stack_add_view(app->submenu_stack, submenu_get_view(app->submenu));
    view_stack_add_view(app->submenu_stack, app->submenu_input_view);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, nlx_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nlx_navigation_event_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, NlxViewSubmenu, view_stack_get_view(app->submenu_stack));
    view_dispatcher_add_view(app->view_dispatcher, NlxViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher, NlxViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void nlx_app_free(NlxElectricalApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, NlxViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, NlxViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, NlxViewTextInput);
    view_stack_remove_view(app->submenu_stack, app->submenu_input_view);
    view_stack_remove_view(app->submenu_stack, submenu_get_view(app->submenu));
    view_stack_free(app->submenu_stack);
    view_free(app->submenu_input_view);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t voltcalc_app(void* context) {
    UNUSED(context);
    NlxElectricalApp* app = nlx_app_alloc();
    scene_manager_next_scene(app->scene_manager, NlxSceneMain);
    view_dispatcher_run(app->view_dispatcher);
    nlx_app_free(app);
    return 0;
}
