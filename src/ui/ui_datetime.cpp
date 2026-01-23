#include <lvgl.h>
#include <cstdio>
#include <time.h>
#include <sys/time.h>

#include "ui/ui.h"
#include "ui/ui_datetime.h"
#include "components/ui_comp.h"
#include "components/ui_comp_topBar.h"

// ====== Config ======
static int s_year_start = 2024;
static int s_year_count = 15;

static bool s_format_24h = true;
static bool s_is_pm = false;

// Si estás editando en ScreenDate, puedes evitar que el timer pise controles
static bool s_editing = false;

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
        // en 12h sigues usando s_is_pm + valor mostrado (01..12)
        int h12 = roller_get_int(ui_RollerH); // "01".."12"
        bool pm = s_is_pm;
        if (!pm) return (h12 == 12) ? 0 : h12;
        else     return (h12 == 12) ? 12 : (h12 + 12);
    }
}


static void set_system_time(int Y,int Mo,int D,int H,int Mi,int S)
{
    struct tm t = {};
    t.tm_year = Y - 1900;
    t.tm_mon  = Mo - 1;
    t.tm_mday = D;
    t.tm_hour = H;
    t.tm_min  = Mi;
    t.tm_sec  = S;

    time_t epoch = mktime(&t);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
}

// Lee time() y actualiza variables internas + labels
static void load_internal_from_system_time()
{
    time_t now = time(nullptr);
    if (now < 100000) return; // no seteado

    struct tm tmnow;
    localtime_r(&now, &tmnow);

    sY  = tmnow.tm_year + 1900;
    sMo = tmnow.tm_mon + 1;
    sD  = tmnow.tm_mday;
    sH  = tmnow.tm_hour;
    sMi = tmnow.tm_min;
    sS  = tmnow.tm_sec;
}

static void datetime_timer_cb(lv_timer_t *t)
{
    (void)t;

    // Si estás editando ScreenDate, no toques los rollers.
    // Pero si quieres, puedes seguir refrescando el preview con lo editado:
    if (s_editing) {
        // mantiene vivo el preview mientras editas
        if (ui_LabelPreviewDateTime) ui_datetime_refresh_preview_label();
        return;
    }

    load_internal_from_system_time();
    render_datetime_current();
}

// ====== Public API ======
void ui_datetime_init_controls(void)
{
    build_years_options(2024, 15);

    lv_roller_set_options(ui_RollerMes, MONTHS_12, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_options(ui_RollerH, HOURS_24,   LV_ROLLER_MODE_NORMAL);

    build_days_options(31);

    lv_roller_set_selected(ui_RollerAno, 0, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerMes, 0, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerDia, 0, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerH,   0, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerM,   0, LV_ANIM_OFF);
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
}

// ✅ NUEVO: marca si estás editando en ScreenDate
void ui_datetime_set_editing(bool editing)
{
    s_editing = editing;
}

// ✅ NUEVO: carga los rollers desde la hora real del sistema (que viene del DS3231 al boot)
void ui_datetime_load_from_system_to_controls(void)
{
    load_internal_from_system_time();

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

    // Hora / Min
    if (s_format_24h) {
        lv_roller_set_selected(ui_RollerH, (uint16_t)sH, LV_ANIM_OFF);
    } else {
        bool pm = (sH >= 12);
        s_is_pm = pm;
        int h12 = sH % 12; if (h12 == 0) h12 = 12;
        lv_roller_set_selected(ui_RollerH, (uint16_t)(h12 - 1), LV_ANIM_OFF);
        // si usas dropdown AM/PM:
        // lv_dropdown_set_selected(ui_DropdownAmPm, pm ? 1 : 0);
    }

    lv_roller_set_selected(ui_RollerM, (uint16_t)sMi, LV_ANIM_OFF);

    ui_datetime_refresh_preview_label();
}

// ✅ NUEVO: carga rollers desde el reloj interno (sY,sMo,sD,sH,sMi) SIN time()
void ui_datetime_load_from_current_to_controls(void)
{
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

    // Hora / Min
    if (s_format_24h) {
        lv_roller_set_selected(ui_RollerH, (uint16_t)sH, LV_ANIM_OFF);
    } else {
        bool pm = (sH >= 12);
        s_is_pm = pm;
        int h12 = sH % 12; if (h12 == 0) h12 = 12;
        lv_roller_set_selected(ui_RollerH, (uint16_t)(h12 - 1), LV_ANIM_OFF);
    }

    lv_roller_set_selected(ui_RollerM, (uint16_t)sMi, LV_ANIM_OFF);

    ui_datetime_refresh_preview_label();
}

void ui_datetime_refresh_preview_label(void)
{
    int Y  = get_year_ui();
    int Mo = get_month_ui();
    int D  = get_day_ui();
    int h24 = get_hour24_ui();
    int mi = get_min_ui();

    char buf[64];

    if (!s_format_24h) {
        int h12 = (int)lv_roller_get_selected(ui_RollerH) + 1;
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

// ✅ NUEVO: aplica lo editado por UI al reloj del sistema
// (y refresca labels). El guardado al DS3231 lo haces donde prefieras.
void ui_datetime_apply_controls_to_system_time(int seconds /*=0*/)
{
    int Y,Mo,D,h24,mi;
    ui_datetime_get_values(&Y,&Mo,&D,&h24,&mi);

    set_system_time(Y,Mo,D,h24,mi,seconds);

    // actualiza interno + labels
    load_internal_from_system_time();
    render_datetime_current();
}

void ui_datetime_start_timer(void)
{
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
