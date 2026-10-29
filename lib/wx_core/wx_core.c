#include "wx_core.h"

#include <math.h>
#include <stdio.h>

/* rain board thresholds, measured with a spray bottle on the FC-37 pad */
#define RAIN_DRY_MIN     3000
#define RAIN_LIGHT_MIN   1500

#define ADC_MAX          4095

int wx_validate(const wx_reading_t *r)
{
    int flags = WX_OK;

    if (isnan(r->temp_c) || r->temp_c < -40.0f || r->temp_c > 85.0f)
        flags |= WX_BAD_TEMP;
    if (isnan(r->humidity) || r->humidity < 0.0f || r->humidity > 100.0f)
        flags |= WX_BAD_HUM;
    if (isnan(r->pressure_hpa) || r->pressure_hpa < 300.0f ||
        r->pressure_hpa > 1100.0f)
        flags |= WX_BAD_PRESS;

    return flags;
}

float wx_dew_point(float temp_c, float rh)
{
    const float a = 17.62f;
    const float b = 243.12f;
    float g;

    if (isnan(temp_c) || isnan(rh) || rh <= 0.0f)
        return NAN;

    g = logf(rh / 100.0f) + (a * temp_c) / (b + temp_c);
    return (b * g) / (a - g);
}

const char *wx_rain_level(uint16_t raw)
{
    if (raw >= RAIN_DRY_MIN)
        return "dry";
    if (raw >= RAIN_LIGHT_MIN)
        return "light";
    return "heavy";
}

int wx_light_percent(uint16_t raw)
{
    if (raw > ADC_MAX)
        raw = ADC_MAX;
    /* round to nearest instead of truncating */
    return (int)((raw * 100u + ADC_MAX / 2) / ADC_MAX);
}

static int put_float(char *buf, size_t len, const char *key, float v,
                     int ok, int last)
{
    const char *sep = last ? "" : ",";

    if (!ok)
        return snprintf(buf, len, "\"%s\":null%s", key, sep);
    return snprintf(buf, len, "\"%s\":%.1f%s", key, (double)v, sep);
}

int wx_to_json(const wx_reading_t *r, const char *station_id,
               char *buf, size_t len)
{
    int bad = wx_validate(r);
    float dew = NAN;
    size_t used = 0;
    int n;

    if (!(bad & (WX_BAD_TEMP | WX_BAD_HUM)))
        dew = wx_dew_point(r->temp_c, r->humidity);

#define APPEND(expr)                                   \
    do {                                               \
        n = (expr);                                    \
        if (n < 0 || (size_t)n >= len - used)          \
            return -1;                                 \
        used += (size_t)n;                             \
    } while (0)

    APPEND(snprintf(buf + used, len - used, "{\"id\":\"%s\",", station_id));
    APPEND(put_float(buf + used, len - used, "temp_c", r->temp_c,
                     !(bad & WX_BAD_TEMP), 0));
    APPEND(put_float(buf + used, len - used, "humidity", r->humidity,
                     !(bad & WX_BAD_HUM), 0));
    APPEND(put_float(buf + used, len - used, "pressure_hpa", r->pressure_hpa,
                     !(bad & WX_BAD_PRESS), 0));
    APPEND(put_float(buf + used, len - used, "dew_point_c", dew,
                     !isnan(dew), 0));
    APPEND(snprintf(buf + used, len - used,
                    "\"rain\":\"%s\",\"rain_raw\":%u,\"light_pct\":%d,"
                    "\"uptime_s\":%lu}",
                    wx_rain_level(r->rain_raw), (unsigned)r->rain_raw,
                    wx_light_percent(r->light_raw),
                    (unsigned long)r->uptime_s));
#undef APPEND

    return (int)used;
}
