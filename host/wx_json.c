/*
 * Small host tool: reads "temp hum press rain_raw light_raw uptime" lines
 * from stdin and prints the same JSON the firmware sends. Used by the
 * Python tests to make sure the logger understands the real C output.
 * Use "nan" for a failed sensor.
 */
#include <stdio.h>
#include <stdlib.h>

#include "wx_core.h"

int main(void)
{
    char line[128];
    char json[256];

    while (fgets(line, sizeof(line), stdin)) {
        wx_reading_t r;
        unsigned rain, light;
        unsigned long up;

        if (sscanf(line, "%f %f %f %u %u %lu", &r.temp_c, &r.humidity,
                   &r.pressure_hpa, &rain, &light, &up) != 6) {
            fprintf(stderr, "bad line: %s", line);
            continue;
        }
        r.rain_raw = (uint16_t)rain;
        r.light_raw = (uint16_t)light;
        r.uptime_s = (uint32_t)up;

        if (wx_to_json(&r, "wx01", json, sizeof(json)) < 0)
            return 1;
        puts(json);
    }
    return 0;
}
