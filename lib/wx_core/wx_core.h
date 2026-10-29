/*
 * wx_core - sensor value handling for the weather station.
 *
 * Kept as plain C with no Arduino includes so the same code builds on the
 * ESP32 and on my laptop for unit tests.
 */
#ifndef WX_CORE_H
#define WX_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temp_c;        /* NAN if the read failed */
    float humidity;      /* %RH, NAN if the read failed */
    float pressure_hpa;  /* NAN if no pressure sensor */
    uint16_t rain_raw;   /* 12-bit ADC, 4095 = dry */
    uint16_t light_raw;  /* 12-bit ADC, higher = brighter */
    uint32_t uptime_s;
} wx_reading_t;

/* bit flags returned by wx_validate() */
#define WX_OK          0
#define WX_BAD_TEMP    (1 << 0)
#define WX_BAD_HUM     (1 << 1)
#define WX_BAD_PRESS   (1 << 2)

int wx_validate(const wx_reading_t *r);

/* Magnus formula, good to about +-0.4 C between -40 and 50 C */
float wx_dew_point(float temp_c, float rh);

const char *wx_rain_level(uint16_t raw);
int wx_light_percent(uint16_t raw);

/*
 * Writes one JSON object into buf. Bad fields are written as null.
 * Returns number of chars written, or -1 if buf was too small.
 */
int wx_to_json(const wx_reading_t *r, const char *station_id,
               char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
