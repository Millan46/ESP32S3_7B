#include <lvgl.h>
#include <cstdio>
#include "ui.h"
#include "ui_datetime.h"
#include "components/ui_comp.h"
#include "components/ui_comp_topBar.h"
#include "clock/clock_manager.h"

// ====== Config ======
static int s_year_start = 2024;
static int s_year_count = 15;

static bool s_format_24h = false;
static bool s_is_pm = false;

// Si estás editando en ScreenDate, puedes evitar que el timer pise controles
static bool s_editing = false;
static bool s_loading_controls = false;

static char s_years_buf[512];
static char s_days_buf[160];

static const char *HOURS_12 =
"01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12";

static const char *HOURS_24 =
"00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";

static const char *MONTHS_12 =
"01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12";

static lv_timer_t* s_dt_timer = nullptr;

// “hora actual” que vamos a mostrar (se actualiza desde time())
static int sY = 2026;
static int sMo = 1;
static int sD = 1;
static int sH = 0;
static int sMi = 0;
static int sS = 0;

// ====== Helpers ======

static bool ui_get_is_pm_from_drop()
{
    // AJUSTA si tu dropdown es 0=AM, 1=PM
    // Si no tienes dropdown, aquí puedes volver a s_is_pm.
    if (ui_DropdownAmPm) {
        return (lv_dropdown_get_selected(ui_DropdownAmPm) == 1);
    }
    return s_is_pm;
}
 
static bool is_leap_year(int y) {
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

static int days_in_month(int y, int m) { // m 1..12
    static const int d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2) return d[1] + (is_leap_year(y) ? 1 : 0);
    return d[m - 1];
}


static lv_obj_t* topbar_find_datetime_label(lv_obj_t* root)
{
    if (!root) return nullptr;

    lv_obj_t* lbl = ui_comp_get_child(root, UI_COMP_TOPBAR_LABELDATETIME);
    if (lbl) return lbl;

    uint32_t cnt = lv_obj_get_child_cnt(root);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* ch = lv_obj_get_child(root, i);
        lbl = topbar_find_datetime_label(ch);
        if (lbl) return lbl;
    }
    return nullptr;
}

static void render_datetime_current(void)
{
    char buf[64];

    if (!s_format_24h) {
        int h12 = sH % 12;
        if (h12 == 0) h12 = 12;
        bool pm = (sH >= 12);

        snprintf(buf, sizeof(buf),
                 "%02d/%02d/%04d  %02d:%02d %s",
                 sD, sMo, sY, h12, sMi, pm ? "PM" : "AM");
    } else {
        snprintf(buf, sizeof(buf),
                 "%02d/%02d/%04d  %02d:%02d",
                 sD, sMo, sY, sH, sMi);
    }

    // ✅ Actualiza el topbar del screen activo (cualquier screen)
    lv_obj_t* scr = lv_scr_act();
    lv_obj_t* lblTop = topbar_find_datetime_label(scr);
    if (lblTop) {
        lv_label_set_text(lblTop, buf);
    }

    // ✅ Preview (solo existe en ScreenDate)
    if (ui_LabelPreviewDateTime) {
        lv_label_set_text(ui_LabelPreviewDateTime, buf);
    }
}

static void build_years_options(int start_year, int count)
{
    s_year_start = start_year;
    s_year_count = count;

    size_t pos = 0;
    s_years_buf[0] = '\0';

    for (int i = 0; i < count; i++) {
        int y = start_year + i;
        int n = std::snprintf(&s_years_buf[pos], sizeof(s_years_buf) - pos,
                              (i == 0) ? "%d" : "\n%d", y);
        if (n <= 0) break;
        pos += (size_t)n;
        if (pos >= sizeof(s_years_buf) - 1) break;
    }

    lv_roller_set_options(ui_RollerAno, s_years_buf, LV_ROLLER_MODE_NORMAL);
}

static void build_days_options(int day_count)
{
    size_t pos = 0;
    s_days_buf[0] = '\0';

    for (int d = 1; d <= day_count; d++) {
        int n = std::snprintf(&s_days_buf[pos], sizeof(s_days_buf) - pos,
                              (d == 1) ? "%02d" : "\n%02d", d);
        if (n <= 0) break;
        pos += (size_t)n;
        if (pos >= sizeof(s_days_buf) - 1) break;
    }

    lv_roller_set_options(ui_RollerDia, s_days_buf, LV_ROLLER_MODE_NORMAL);
}

static int roller_get_int(lv_obj_t *roller)
{
    char buf[12] = {0};
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    return atoi(buf);
}

static int get_year_ui()  { return roller_get_int(ui_RollerAno); }  // "2024"..."2039"
static int get_month_ui() { return roller_get_int(ui_RollerMes); }  // "01"..."12"
static int get_day_ui()   { return roller_get_int(ui_RollerDia); }  // "01"..."31"
static int get_min_ui()   { return roller_get_int(ui_RollerM); }    // "00"..."59"

static int get_hour24_ui()
{
    if (s_format_24h) {
        return roller_get_int(ui_RollerH); // "00".."23"
    } else {
        int h12 = roller_get_int(ui_RollerH); // "01".."12"
        bool pm = ui_get_is_pm_from_drop();

        if (!pm) return (h12 == 12) ? 0 : h12;
        else     return (h12 == 12) ? 12 : (h12 + 12);
    }
}


static void datetime_timer_cb(lv_timer_t *t)
{
    (void)t;

    // Si estás editando, NO pises rollers/preview
    if (s_editing) {
        if (ui_LabelPreviewDateTime) ui_datetime_refresh_preview_label();
        return;
    }

    // ✅ Fuente de verdad: clock_manager
    ClockDateTime dt;
    if (clock_manager_get_now(dt)) {
        // actualiza cache local para render/topbar
        ui_datetime_set_current(dt.y, dt.mo, dt.d, dt.h, dt.mi, dt.s);
        render_datetime_current();
    } else {
        // Si no hay tiempo válido, al menos renderiza lo que tengas
        render_datetime_current();
    }
}

// ====== Public API ======
void ui_datetime_init_controls(void)
{
    s_loading_controls = true;

    build_years_options(2024, 15);
    build_days_options(31);

    lv_roller_set_options(ui_RollerMes, MONTHS_12, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_options(ui_RollerH,   HOURS_12,  LV_ROLLER_MODE_NORMAL);

    int Y, Mo, D, h24, mi;
    if (!clock_manager_get_now_ymdhm(&Y, &Mo, &D, &h24, &mi)) {
        s_loading_controls = false;
        return;
    }

    int year_index = Y - 2024;
    if (year_index < 0) year_index = 0;
    if (year_index > 14) year_index = 14;

    lv_roller_set_selected(ui_RollerAno, year_index, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerMes, Mo - 1,     LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerDia, D  - 1,     LV_ANIM_OFF);

    // 12h
    s_format_24h = false;
    s_is_pm = (h24 >= 12);

    int h12 = h24 % 12;
    if (h12 == 0) h12 = 12;

    lv_roller_set_selected(ui_RollerH, h12 - 1, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerM, mi,      LV_ANIM_OFF);

    // si tienes dropdown AM/PM, aquí debes setearlo acorde a s_is_pm
    // ui_set_drop_pm(s_is_pm);  <-- dime cómo se llama tu dropdown y te lo pongo exacto

    s_loading_controls = false;
    ui_datetime_refresh_preview_label();
}

void ui_datetime_refresh_days_keep_selection(void)
{
    int y = get_year_ui();
    int m = get_month_ui();
    int maxd = days_in_month(y, m);

    int cur_day = get_day_ui();
    build_days_options(maxd);

    if (cur_day > maxd) cur_day = maxd;
    lv_roller_set_selected(ui_RollerDia, (uint16_t)(cur_day - 1), LV_ANIM_OFF);
}

void ui_datetime_set_format_24h(bool is24)
{
    s_format_24h = is24;

    lv_roller_set_options(ui_RollerH, is24 ? HOURS_24 : HOURS_12, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(ui_RollerH, 0, LV_ANIM_OFF);
}

void ui_datetime_set_pm(bool pm)
{
    s_is_pm = pm;
    if (ui_DropdownAmPm) lv_dropdown_set_selected(ui_DropdownAmPm, pm ? 1 : 0);
    ui_datetime_refresh_preview_label();
}


// ✅ NUEVO: marca si estás editando en ScreenDate
void ui_datetime_set_editing(bool editing)
{
    s_editing = editing;
}

void ui_datetime_load_from_current_to_controls(void)
{
    s_loading_controls = true;
    
    int Y, Mo, D, h24, mi;
    if (clock_manager_get_now_ymdhm(&Y, &Mo, &D, &h24, &mi)) {
        sY  = Y;
        sMo = Mo;
        sD  = D;
        sH  = h24;
        sMi = mi;
    }


    // Año
    int y_idx = sY - s_year_start;
    if (y_idx < 0) y_idx = 0;
    if (y_idx >= s_year_count) y_idx = s_year_count - 1;
    lv_roller_set_selected(ui_RollerAno, (uint16_t)y_idx, LV_ANIM_OFF);

    // Mes
    lv_roller_set_selected(ui_RollerMes, (uint16_t)(sMo - 1), LV_ANIM_OFF);

    // Días
    int maxd = days_in_month(sY, sMo);
    build_days_options(maxd);
    int dd = sD;
    if (dd > maxd) dd = maxd;
    lv_roller_set_selected(ui_RollerDia, (uint16_t)(dd - 1), LV_ANIM_OFF);

    // Min
    lv_roller_set_selected(ui_RollerM, (uint16_t)sMi, LV_ANIM_OFF);

    // Hora + AM/PM
    if (s_format_24h) {
        lv_roller_set_options(ui_RollerH, HOURS_24, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(ui_RollerH, (uint16_t)sH, LV_ANIM_OFF);
    } else {
        lv_roller_set_options(ui_RollerH, HOURS_12, LV_ROLLER_MODE_NORMAL);

        bool pm = (sH >= 12);
        int h12 = sH % 12; if (h12 == 0) h12 = 12;

        // ✅ esto es lo que te falta:
        if (ui_DropdownAmPm) lv_dropdown_set_selected(ui_DropdownAmPm, pm ? 1 : 0);

        // por compatibilidad, mantén también la variable:
        s_is_pm = pm;

        lv_roller_set_selected(ui_RollerH, (uint16_t)(h12 - 1), LV_ANIM_OFF);
    }

    s_loading_controls = false;

    // ✅ al final, ya con todo seteado
    ui_datetime_refresh_preview_label();
}


void ui_datetime_refresh_preview_label(void)
{ 
    if (s_loading_controls) return;

    int Y  = get_year_ui();
    int Mo = get_month_ui();
    int D  = get_day_ui();
    int h24 = get_hour24_ui();
    int mi = get_min_ui();

    char buf[64];

    if (!s_format_24h) {
        int h12 = (int)lv_roller_get_selected(ui_RollerH) + 1;
        bool pm = ui_get_is_pm_from_drop();  
        s_is_pm = pm; // <-- clave para mantener todo coherente
        std::snprintf(buf, sizeof(buf),
                      "%02d/%02d/%04d  %02d:%02d %s",
                      D, Mo, Y, h12, mi, s_is_pm ? "PM" : "AM");
    } else {
        std::snprintf(buf, sizeof(buf),
                      "%02d/%02d/%04d  %02d:%02d",
                      D, Mo, Y, h24, mi);
    }

    if (ui_LabelPreviewDateTime) lv_label_set_text(ui_LabelPreviewDateTime, buf);

    if (ui_topBar) {
       lv_obj_t *lblTop = ui_comp_get_child(ui_topBar, UI_COMP_TOPBAR_LABELDATETIME);
       if (lblTop) lv_label_set_text(lblTop, buf);
    }
}

void ui_datetime_get_values(int *Y,int *Mo,int *D,int *h24,int *mi)
{
    if (Y)   *Y   = get_year_ui();
    if (Mo)  *Mo  = get_month_ui();
    if (D)   *D   = get_day_ui();
    if (h24) *h24 = get_hour24_ui();
    if (mi)  *mi  = get_min_ui();
}


void ui_datetime_start_timer(void)
{
    // sync inicial
    ClockDateTime dt;
    if (clock_manager_get_now(dt)) {
        ui_datetime_set_current(dt.y, dt.mo, dt.d, dt.h, dt.mi, dt.s);
        render_datetime_current();
    }

    if (!s_dt_timer) {
        s_dt_timer = lv_timer_create(datetime_timer_cb, 1000, NULL);
    }
}


void ui_datetime_stop_timer(void)
{
    if (s_dt_timer) {
        lv_timer_del(s_dt_timer);
        s_dt_timer = nullptr;
    }
}
void ui_datetime_render_now(void)
{
    render_datetime_current();
}
void ui_datetime_set_current(int Y,int Mo,int D,int h24,int mi,int sec)
{
    sY  = Y;
    sMo = Mo;
    sD  = D;
    sH  = h24;
    sMi = mi;
    sS  = sec;

    // opcional: refresca label al instante si ya hay UI
    // render_datetime_current();
}

void ui_datetime_get_current(int *Y,int *Mo,int *D,int *h24,int *mi,int *sec)
{
    if (Y)   *Y   = sY;
    if (Mo)  *Mo  = sMo;
    if (D)   *D   = sD;
    if (h24) *h24 = sH;
    if (mi)  *mi  = sMi;
    if (sec) *sec = sS;
}
