#include <pebble.h>

/* ── UI elements ─────────────────────────────────────────────── */
static Window    *s_main_window;
static TextLayer *s_date_layer;
static Layer     *s_battery_layer;
static Layer     *s_weather_layer;
static Layer     *s_calendar_layer;

/* ── Custom font ─────────────────────────────────────────────── */
static GFont s_weather_font;

/* ── Weather data ────────────────────────────────────────────── */
static int  s_temp        = -99;
static int  s_temp_min    = -99;
static int  s_temp_max    = -99;
static int  s_wind        = 0;
static int  s_weather_code = -1;
static char s_sunrise_buf[8]  = "--:--";
static char s_sunset_buf[8]   = "--:--";

static int  s_fc_temp     = -99;
static int  s_fc_temp_min = -99;
static int  s_fc_temp_max = -99;
static int  s_fc_wind     = 0;
static int  s_fc_weather_code = -1;
static char s_fc_sunrise_buf[8] = "--:--";
static char s_fc_sunset_buf[8]  = "--:--";

static time_t s_last_update  = 0;
static int    s_current_hour = 0;

/* ── Buffers ─────────────────────────────────────────────────── */
static char s_date_buf[16];

/* ── Battery state ───────────────────────────────────────────── */
static uint8_t s_battery_pct = 0;

/* ================================================================
 *  WEATHER ICONS FONT CHARACTER MAPPING
 *  Font: Weather Icons by Erik Flowers (SIL OFL)
 *  All glyphs in Unicode Private Use Area (U+F0xx)
 * ================================================================ */

/* Weather condition icons (WMO code → font character) */
static const char* wmo_to_icon_str(int wmo_code) {
  switch (wmo_code) {
    case 0:  return "\uf00d";  /* wi-day-sunny */
    case 1:  return "\uf00d";  /* wi-day-sunny (mainly clear) */
    case 2:  return "\uf002";  /* wi-day-cloudy (partly cloudy) */
    case 3:  return "\uf013";  /* wi-cloudy (overcast) */
    case 45: return "\uf014";  /* wi-fog */
    case 48: return "\uf014";  /* wi-fog (rime) */
    case 51: return "\uf01c";  /* wi-sprinkle (light drizzle) */
    case 53: return "\uf01c";  /* wi-sprinkle (moderate drizzle) */
    case 55: return "\uf01a";  /* wi-showers (dense drizzle) */
    case 56: return "\uf017";  /* wi-rain-mix (freezing drizzle) */
    case 57: return "\uf017";  /* wi-rain-mix */
    case 61: return "\uf008";  /* wi-day-rain (slight) */
    case 63: return "\uf019";  /* wi-rain (moderate) */
    case 65: return "\uf019";  /* wi-rain (heavy) */
    case 66: return "\uf017";  /* wi-rain-mix (freezing) */
    case 67: return "\uf017";  /* wi-rain-mix */
    case 71: return "\uf00a";  /* wi-day-snow (slight) */
    case 73: return "\uf01b";  /* wi-snow (moderate) */
    case 75: return "\uf01b";  /* wi-snow (heavy) */
    case 77: return "\uf076";  /* wi-snowflake-cold (grains) */
    case 80: return "\uf01a";  /* wi-showers (slight) */
    case 81: return "\uf01a";  /* wi-showers (moderate) */
    case 82: return "\uf019";  /* wi-rain (violent) */
    case 85: return "\uf01b";  /* wi-snow (slight showers) */
    case 86: return "\uf01b";  /* wi-snow (heavy showers) */
    case 95: return "\uf01e";  /* wi-thunderstorm */
    case 96: return "\uf01d";  /* wi-storm-showers (+ hail) */
    case 99: return "\uf015";  /* wi-hail */
    default: return "\uf013";  /* wi-cloudy (fallback) */
  }
}

/* Icon string constants */
#define WI_SUNRISE      "\uf051"
#define WI_SUNSET       "\uf052"
/* wind and direction arrows don't render on Pebble from this font,
 * so we use plain text from the system font instead */

/* ================================================================
 *  BATTERY DRAWING
 * ================================================================ */
#define BATT_W  22
#define BATT_H  11
#define NUB_W    2
#define NUB_H    5
#define SEG_GAP  1
#define SEG_N    4

static void battery_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  /* nub on left, body on right */
  int x_nub = b.size.w - BATT_W - NUB_W - 2;
  int x0 = x_nub + NUB_W;               /* body starts after nub */
  int y0 = (b.size.h - BATT_H) / 2;

  /* nub (left side) */
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x_nub, y0 + (BATT_H - NUB_H) / 2,
                                NUB_W, NUB_H), 0, GCornerNone);
  /* body outline */
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_rect(ctx, GRect(x0, y0, BATT_W, BATT_H));

  int inner_w = BATT_W - 2;
  int seg_w   = (inner_w - (SEG_N - 1) * SEG_GAP) / SEG_N;
  int filled  = (s_battery_pct >= 88) ? 4 :
                (s_battery_pct >= 63) ? 3 :
                (s_battery_pct >= 38) ? 2 :
                (s_battery_pct >= 13) ? 1 : 0;

  /* fill segments right-to-left (full = rightmost) */
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < filled; i++) {
    int sx = x0 + inner_w - (i + 1) * seg_w - i * SEG_GAP + 1;
    graphics_fill_rect(ctx, GRect(sx, y0 + 2, seg_w, BATT_H - 4),
                       0, GCornerNone);
  }
}

static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent;
  if (s_battery_layer) layer_mark_dirty(s_battery_layer);
}

/* ================================================================
 *  WEATHER LAYER DRAWING
 * ================================================================
 *  Row 1: [weather_icon] 12°  [▲]18° [▼]5°  [wind_icon] 15
 *  Row 2: [sunrise/sunset_icon] 06:32       upd 15m
 * ================================================================ */
#define WX_ROW_H  18
#define WX_ICON_W 18  /* icon character width with padding */

static void weather_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  int w = b.size.w;

  bool tomorrow = (s_current_hour >= 20);
  int  temp     = tomorrow ? s_fc_temp     : s_temp;
  int  tmin     = tomorrow ? s_fc_temp_min : s_temp_min;
  int  tmax     = tomorrow ? s_fc_temp_max : s_temp_max;
  int  wind     = tomorrow ? s_fc_wind     : s_wind;
  int  wcode    = tomorrow ? s_fc_weather_code : s_weather_code;
  const char *sunrise = tomorrow ? s_fc_sunrise_buf : s_sunrise_buf;
  const char *sunset  = tomorrow ? s_fc_sunset_buf  : s_sunset_buf;

  /* ── Row 1: condition icon + temps + wind ── */
  int y1 = 0;
  int x = 0;

  /* "Tmrw" label if showing tomorrow */
  if (tomorrow) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "Tmrw", font,
        GRect(x, y1, 30, WX_ROW_H),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    x += 30;
  }

  /* weather condition icon from Weather Icons font */
  if (wcode >= 0) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, wmo_to_icon_str(wcode), s_weather_font,
        GRect(x, y1 - 2, WX_ICON_W, WX_ROW_H),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
  x += WX_ICON_W;

  /* current temp */
  char temp_buf[12];
  if (temp > -99)
    snprintf(temp_buf, sizeof(temp_buf), "%d\xc2\xb0", temp);
  else
    snprintf(temp_buf, sizeof(temp_buf), "--");
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, temp_buf, font,
      GRect(x, y1, 26, WX_ROW_H),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  x += 24;

  /* hi/lo temps + wind — using system font with plain text labels */
  char stats_buf[32];
  if (tmax > -99 && tmin > -99) {
    snprintf(stats_buf, sizeof(stats_buf), "H%d\xc2\xb0 L%d\xc2\xb0 W%dkm/h",
             tmax, tmin, wind);
  } else {
    snprintf(stats_buf, sizeof(stats_buf), "--/-- W%dkm/h", wind);
  }
  graphics_draw_text(ctx, stats_buf, font,
      GRect(x, y1, w - x, WX_ROW_H),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  /* ── Row 2: sunrise/sunset icon + time + relative update ── */
  int y2 = WX_ROW_H;
  x = 0;
  if (tomorrow) x += 30;

  bool show_sunrise;
  const char *sun_val;
  if (tomorrow) {
    show_sunrise = true;
    sun_val = sunrise;
  } else {
    show_sunrise = (s_current_hour < 14);
    sun_val = show_sunrise ? sunrise : sunset;
  }

  /* sunrise or sunset icon from Weather Icons font */
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx,
      show_sunrise ? WI_SUNRISE : WI_SUNSET,
      s_weather_font,
      GRect(x, y2 - 2, WX_ICON_W, WX_ROW_H),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  x += WX_ICON_W;

  /* sunrise/sunset time */
  graphics_draw_text(ctx, sun_val, font,
      GRect(x, y2, 40, WX_ROW_H),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  x += 40;

  /* relative update time */
  char upd_buf[20] = "";
  if (s_last_update > 0) {
    int diff = (int)difftime(time(NULL), s_last_update);
    if (diff < 60)
      snprintf(upd_buf, sizeof(upd_buf), "upd %ds", diff);
    else if (diff < 3600)
      snprintf(upd_buf, sizeof(upd_buf), "upd %dm", diff / 60);
    else
      snprintf(upd_buf, sizeof(upd_buf), "upd %dh", diff / 3600);
  }
  graphics_draw_text(ctx, upd_buf, font,
      GRect(x, y2, w - x - 2, WX_ROW_H),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

/* ================================================================
 *  CALENDAR DRAWING
 * ================================================================ */
#define CAL_COLS    7
#define CAL_ROWS    4
#define CAL_HDR_H  18
#define CAL_ROW_H  15
#define CAL_TOP     0

static const char *s_day_names[] = {"Mo","Tu","We","Th","Fr","Sa","Su"};

static void calendar_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int cell_w = b.size.w / CAL_COLS;

  GFont hdr_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont day_font       = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont day_font_bold  = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

  /* ── draw grid lines ─────────────────────── */
  int grid_w = CAL_COLS * cell_w;          /* actual grid width */
  int grid_top = CAL_TOP + CAL_HDR_H;
  int grid_bot = grid_top + CAL_ROWS * CAL_ROW_H;
  graphics_context_set_stroke_color(ctx, GColorWhite);

  /* horizontal lines (top of each row + bottom) */
  for (int r = 0; r <= CAL_ROWS; r++) {
    int ly = grid_top + r * CAL_ROW_H;
    if (ly >= b.size.h) ly = b.size.h - 1; /* keep bottom line visible */
    graphics_draw_line(ctx, GPoint(0, ly), GPoint(grid_w, ly));
  }

  /* vertical lines */
  for (int c = 0; c <= CAL_COLS; c++) {
    int lx = c * cell_w;
    graphics_draw_line(ctx, GPoint(lx, grid_top), GPoint(lx, grid_bot));
  }

  /* ── header row ────────────────────────────── */
  graphics_context_set_text_color(ctx, GColorWhite);
  for (int c = 0; c < CAL_COLS; c++) {
    GRect cell = GRect(c * cell_w, CAL_TOP, cell_w, CAL_HDR_H);
    graphics_draw_text(ctx, s_day_names[c], hdr_font, cell,
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  time_t now = time(NULL);
  struct tm *today = localtime(&now);
  int today_mday = today->tm_mday;
  int today_mon  = today->tm_mon;
  int today_year = today->tm_year;

  int wday_mon = (today->tm_wday + 6) % 7;
  int days_back = wday_mon + 7;

  struct tm start = *today;
  start.tm_mday -= days_back;
  start.tm_isdst = -1;
  mktime(&start);

  for (int r = 0; r < CAL_ROWS; r++) {
    int y = CAL_TOP + CAL_HDR_H + r * CAL_ROW_H;
    for (int c = 0; c < CAL_COLS; c++) {
      struct tm day = start;
      day.tm_mday += r * 7 + c;
      day.tm_isdst = -1;
      mktime(&day);

      char num[4];
      snprintf(num, sizeof(num), "%d", day.tm_mday);

      GRect cell = GRect(c * cell_w, y, cell_w, CAL_ROW_H);
      /* text rect with bottom padding (shift text up by 2px) */
      GRect text_r = GRect(cell.origin.x, cell.origin.y - 2,
                           cell.size.w, cell.size.h);
      bool is_today = (day.tm_mday == today_mday &&
                       day.tm_mon  == today_mon &&
                       day.tm_year == today_year);

      if (is_today) {
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_rect(ctx, cell, 0, GCornerNone);
        graphics_context_set_text_color(ctx, GColorBlack);
      } else {
        graphics_context_set_text_color(ctx, GColorWhite);
      }

      graphics_draw_text(ctx, num,
                         is_today ? day_font_bold : day_font, text_r,
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    }
  }
}

/* ================================================================
 *  DATE UPDATE
 * ================================================================ */
static void update_date(struct tm *tick_time) {
  strftime(s_date_buf, sizeof(s_date_buf), "%Y-%m-%d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buf);
}

/* ================================================================
 *  APPMESSAGE
 * ================================================================ */
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_TEMPERATURE)))
    s_temp = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_TEMP_MIN)))
    s_temp_min = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_TEMP_MAX)))
    s_temp_max = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_WIND_SPEED)))
    s_wind = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_WEATHER_CODE)))
    s_weather_code = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_SUNRISE)))
    strncpy(s_sunrise_buf, t->value->cstring, sizeof(s_sunrise_buf) - 1);
  if ((t = dict_find(iter, MESSAGE_KEY_SUNSET)))
    strncpy(s_sunset_buf, t->value->cstring, sizeof(s_sunset_buf) - 1);

  if ((t = dict_find(iter, MESSAGE_KEY_FORECAST_TEMP)))
    s_fc_temp = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_FORECAST_TEMP_MIN)))
    s_fc_temp_min = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_FORECAST_TEMP_MAX)))
    s_fc_temp_max = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_FORECAST_WIND)))
    s_fc_wind = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_FORECAST_WEATHER_CODE)))
    s_fc_weather_code = (int)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_FORECAST_SUNRISE)))
    strncpy(s_fc_sunrise_buf, t->value->cstring, sizeof(s_fc_sunrise_buf) - 1);
  if ((t = dict_find(iter, MESSAGE_KEY_FORECAST_SUNSET)))
    strncpy(s_fc_sunset_buf, t->value->cstring, sizeof(s_fc_sunset_buf) - 1);

  if ((t = dict_find(iter, MESSAGE_KEY_UPDATE_TIMESTAMP)))
    s_last_update = (time_t)t->value->int32;

  if (s_weather_layer) layer_mark_dirty(s_weather_layer);
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

/* ================================================================
 *  TICK HANDLER
 * ================================================================ */
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_current_hour = tick_time->tm_hour;
  update_date(tick_time);
  if (s_weather_layer) layer_mark_dirty(s_weather_layer);
  if (s_calendar_layer) layer_mark_dirty(s_calendar_layer);

  if (units_changed & HOUR_UNIT) {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) == APP_MSG_OK) {
      dict_write_uint8(out, 0, 0);
      app_message_outbox_send();
    }
  }
}

/* ================================================================
 *  WINDOW LOAD / UNLOAD
 * ================================================================ */
static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int w = bounds.size.w;
  int y = 0;

  /* load Weather Icons font */
  s_weather_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_WEATHER_ICONS_14));

  /* ── date ──────────────────────────────────── */
  s_date_layer = text_layer_create(GRect(2, y, w - 30, 22));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer,
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(s_date_layer, s_date_buf);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  /* ── battery ───────────────────────────────── */
  s_battery_layer = layer_create(GRect(w - 30, y + 4, 30, 18));
  layer_set_update_proc(s_battery_layer, battery_draw);
  layer_add_child(root, s_battery_layer);
  y += 22;

  /* ── weather (custom drawn: icons + text) ──── */
  int wx_h = WX_ROW_H * 2;
  s_weather_layer = layer_create(GRect(2, y, w - 4, wx_h));
  layer_set_update_proc(s_weather_layer, weather_draw);
  layer_add_child(root, s_weather_layer);
  y += wx_h;

  /* ── calendar ──────────────────────────────── */
  int cal_h = CAL_HDR_H + CAL_ROWS * CAL_ROW_H;
  s_calendar_layer = layer_create(GRect(0, y, w, cal_h));
  layer_set_update_proc(s_calendar_layer, calendar_draw);
  layer_add_child(root, s_calendar_layer);

  /* seed current state */
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_current_hour = t->tm_hour;
  update_date(t);
  battery_handler(battery_state_service_peek());
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_date_layer);
  layer_destroy(s_battery_layer);
  layer_destroy(s_weather_layer);
  layer_destroy(s_calendar_layer);
  fonts_unload_custom_font(s_weather_font);
}

/* ================================================================
 *  INIT / DEINIT
 * ================================================================ */
static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load   = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_open(512, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
