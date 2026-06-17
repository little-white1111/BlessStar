#include <bs/kernel/ui_map/ui_render_desc.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

const char* bs_ui_widget_type_str(bs_ui_widget_type_t t)
{
    switch (t)
    {
    case BS_UI_WIDGET_INPUT:            return "input";
    case BS_UI_WIDGET_SELECT:           return "select";
    case BS_UI_WIDGET_CHECKBOX:         return "checkbox";
    case BS_UI_WIDGET_RADIO:            return "radio";
    case BS_UI_WIDGET_SWITCH:           return "switch";
    case BS_UI_WIDGET_TEXTAREA:         return "textarea";
    case BS_UI_WIDGET_NUMBER:           return "number";
    case BS_UI_WIDGET_DATEPICKER:       return "datepicker";
    case BS_UI_WIDGET_FILE_UPLOAD:      return "file_upload";
    case BS_UI_WIDGET_KEY_VALUE_TABLE:  return "key_value_table";
    case BS_UI_WIDGET_GROUP:            return "group";
    case BS_UI_WIDGET_TABLE:            return "table";
    case BS_UI_WIDGET_REPEATABLE_GROUP: return "repeatable_group";
    case BS_UI_WIDGET_CUSTOM:           return "custom";
    default:                            return "unknown";
    }
}

bs_ui_widget_type_t bs_ui_default_widget_for_type(int schema_type, int has_enum)
{
    switch (schema_type)
    {
    case 7: /* BS_SCHEMA_TYPE_ENUM */
        return BS_UI_WIDGET_SELECT;
    case 0: /* BS_SCHEMA_TYPE_STR */
        return has_enum ? BS_UI_WIDGET_SELECT : BS_UI_WIDGET_INPUT;
    case 1: /* BS_SCHEMA_TYPE_I32 */
    case 2: /* BS_SCHEMA_TYPE_I64 */
    case 3: /* BS_SCHEMA_TYPE_F64 */
        return BS_UI_WIDGET_NUMBER;
    case 4: /* BS_SCHEMA_TYPE_BOOL */
        return BS_UI_WIDGET_CHECKBOX;
    case 6: /* BS_SCHEMA_TYPE_OBJ */
        return BS_UI_WIDGET_GROUP;
    case 5: /* BS_SCHEMA_TYPE_ARR */
        return BS_UI_WIDGET_REPEATABLE_GROUP;
    default:
        return BS_UI_WIDGET_INPUT;
    }
}

void bs_ui_control_free(bs_ui_control_t* ctl)
{
    if (!ctl) return;
    free(ctl->field);
    free(ctl->label);
    free(ctl->group);
    free(ctl->ai_layout_hint);
    free(ctl->visibility);
    free(ctl->default_value);
    free(ctl->validation_ref);
    bs_ui_control_array_free(ctl->children, ctl->children_count);
    ctl->field          = NULL;
    ctl->label          = NULL;
    ctl->group          = NULL;
    ctl->ai_layout_hint = NULL;
    ctl->visibility     = NULL;
    ctl->default_value  = NULL;
    ctl->validation_ref = NULL;
    ctl->children       = NULL;
    ctl->children_count = 0;
}

void bs_ui_control_array_free(bs_ui_control_t* ctls, size_t count)
{
    if (!ctls) return;
    for (size_t i = 0; i < count; i++)
        bs_ui_control_free(&ctls[i]);
    free(ctls);
}

void bs_ui_render_desc_free(bs_ui_render_desc_t* desc)
{
    if (!desc) return;
    free(desc->schema_ref);
    bs_ui_control_array_free(desc->controls, desc->controls_count);
    desc->schema_ref    = NULL;
    desc->controls      = NULL;
    desc->controls_count = 0;
    desc->uidl_version  = 0;
}

#ifdef __cplusplus
}
#endif
