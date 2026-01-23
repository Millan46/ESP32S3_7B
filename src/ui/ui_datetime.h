#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void ui_datetime_init_controls(void);
void ui_datetime_refresh_days_keep_selection(void);
void ui_datetime_set_format_24h(bool is24);
void ui_datetime_set_pm(bool pm);
void ui_datetime_refresh_preview_label(void);
void ui_datetime_get_values(int *Y,int *Mo,int *D,int *h24,int *mi);

void ui_datetime_set_current(int Y,int Mo,int D,int h24,int mi,int sec);
void ui_datetime_get_current(int *Y,int *Mo,int *D,int *h24,int *mi,int *sec);
void ui_datetime_start_timer(void);
void ui_datetime_stop_timer(void);
void ui_datetime_set_editing(bool editing);
void ui_datetime_load_from_system_to_controls(void);
void ui_datetime_load_from_current_to_controls(void);

void ui_datetime_render_now(void);


/* =================================== */

#ifdef __cplusplus
}
#endif
