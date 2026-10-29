#!/usr/bin/env python3
"""Fake weather station for testing without hardware.

Prints JSON lines in the same format as the firmware. Temperature follows
a simple day/night curve, and there is one rain shower in the afternoon.

    python3 tools/sim_station.py --hours 24 | python3 tools/wx_logger.py --stdin
"""
import argparse
import json
import math
import random
import sys
import time


def dew_point(t, rh):
    a, b = 17.62, 243.12
    g = math.log(rh / 100.0) + a * t / (b + t)
    return b * g / (a - g)


def rain_level(raw):
    if raw >= 3000:
        return "dry"
    if raw >= 1500:
        return "light"
    return "heavy"


def make_reading(t_s, start_ts, rng, station="wx01", start_hour=0.0):
    hour = (start_hour + t_s / 3600.0) % 24
    # coldest around 5am, warmest around 3pm
    temp = 24 + 7 * math.sin((hour - 9) / 24 * 2 * math.pi) + rng.gauss(0, 0.2)
    hum = max(20.0, min(98.0, 60 - 1.8 * (temp - 24) + rng.gauss(0, 1)))
    press = 1009 + 1.5 * math.sin(hour / 12 * 2 * math.pi) + rng.gauss(0, 0.1)

    if 16 <= hour < 17.5:
        rain_raw = int(1100 + rng.random() * 900)
        hum = min(98.0, hum + 20)
    else:
        rain_raw = int(3900 + rng.random() * 150)

    light = max(0.0, math.sin((hour - 6) / 12 * math.pi)) if 6 <= hour <= 18 else 0.0
    light_pct = int(round(light * 100))

    return {
        "id": station,
        "ts": start_ts + t_s,
        "temp_c": round(temp, 1),
        "humidity": round(hum, 1),
        "pressure_hpa": round(press, 1),
        "dew_point_c": round(dew_point(temp, hum), 1),
        "rain": rain_level(rain_raw),
        "rain_raw": rain_raw,
        "light_pct": light_pct,
        "uptime_s": int(t_s),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hours", type=float, default=24)
    ap.add_argument("--interval", type=int, default=30, help="seconds")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--mqtt", metavar="HOST", help="publish instead of print")
    ap.add_argument("--realtime", action="store_true")
    args = ap.parse_args()

    rng = random.Random(args.seed)
    start = time.time() - args.hours * 3600 if not args.realtime else time.time()
    steps = int(args.hours * 3600 / args.interval)
    lt = time.localtime(start)
    # line the fake day up with the clock so the plots make sense
    start_hour = lt.tm_hour + lt.tm_min / 60.0

    client = None
    if args.mqtt:
        import paho.mqtt.client as mqtt
        try:
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        except AttributeError:
            client = mqtt.Client()
        client.connect(args.mqtt, 1883)
        client.loop_start()

    for i in range(steps):
        r = make_reading(i * args.interval, start, rng, start_hour=start_hour)
        line = json.dumps(r, separators=(",", ":"))
        if client:
            client.publish("weather/%s/data" % r["id"], line)
        else:
            sys.stdout.write(line + "\n")
        if args.realtime:
            time.sleep(args.interval)


if __name__ == "__main__":
    main()
