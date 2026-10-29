/* Host side tests for wx_core. Plain asserts, no framework needed. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "wx_core.h"

static int failures;
static int checks;

#define CHECK(cond)                                                   \
    do {                                                              \
        checks++;                                                     \
        if (!(cond)) {                                                \
            failures++;                                               \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        }                                                             \
    } while (0)

#define CHECK_NEAR(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) <= (tol))

static wx_reading_t good_reading(void)
{
    wx_reading_t r = {
        .temp_c = 24.3f,
        .humidity = 55.0f,
        .pressure_hpa = 1008.2f,
        .rain_raw = 4000,
        .light_raw = 2048,
        .uptime_s = 120,
    };
    return r;
}

static void test_validate(void)
{
    wx_reading_t r = good_reading();
    CHECK(wx_validate(&r) == WX_OK);

    r.temp_c = NAN;
    CHECK(wx_validate(&r) == WX_BAD_TEMP);

    r = good_reading();
    r.humidity = 101.0f;
    r.pressure_hpa = 0.0f;
    CHECK(wx_validate(&r) == (WX_BAD_HUM | WX_BAD_PRESS));
}

static void test_dew_point(void)
{
    /* reference values from an online dew point calculator */
    CHECK_NEAR(wx_dew_point(25.0f, 60.0f), 16.7f, 0.2);
    CHECK_NEAR(wx_dew_point(10.0f, 100.0f), 10.0f, 0.05);
    CHECK(isnan(wx_dew_point(20.0f, 0.0f)));
}

static void test_rain_and_light(void)
{
    CHECK(strcmp(wx_rain_level(4095), "dry") == 0);
    CHECK(strcmp(wx_rain_level(3000), "dry") == 0);
    CHECK(strcmp(wx_rain_level(2999), "light") == 0);
    CHECK(strcmp(wx_rain_level(800), "heavy") == 0);

    CHECK(wx_light_percent(0) == 0);
    CHECK(wx_light_percent(4095) == 100);
    CHECK(wx_light_percent(2048) == 50);
    CHECK(wx_light_percent(5000) == 100);  /* clamp */
}

static void test_json(void)
{
    char buf[256];
    wx_reading_t r = good_reading();
    int n = wx_to_json(&r, "wx01", buf, sizeof(buf));

    CHECK(n > 0);
    CHECK(n == (int)strlen(buf));
    CHECK(strstr(buf, "\"id\":\"wx01\"") != NULL);
    CHECK(strstr(buf, "\"temp_c\":24.3") != NULL);
    CHECK(strstr(buf, "\"pressure_hpa\":1008.2") != NULL);
    CHECK(strstr(buf, "\"rain\":\"dry\"") != NULL);
    CHECK(buf[n - 1] == '}');
}

static void test_json_nulls(void)
{
    char buf[256];
    wx_reading_t r = good_reading();

    r.humidity = NAN;  /* DHT22 timeout */
    CHECK(wx_to_json(&r, "wx01", buf, sizeof(buf)) > 0);
    CHECK(strstr(buf, "\"humidity\":null") != NULL);
    CHECK(strstr(buf, "\"dew_point_c\":null") != NULL);
}

static void test_json_small_buffer(void)
{
    char buf[40];
    wx_reading_t r = good_reading();

    CHECK(wx_to_json(&r, "wx01", buf, sizeof(buf)) == -1);
}

int main(void)
{
    test_validate();
    test_dew_point();
    test_rain_and_light();
    test_json();
    test_json_nulls();
    test_json_small_buffer();

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
