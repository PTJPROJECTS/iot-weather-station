#!/usr/bin/env python3
"""Store weather station readings in SQLite.

Two input modes:
  --mqtt HOST     subscribe to weather/+/data on the broker
  --stdin         read one JSON object per line (used with sim_station.py)
"""
import argparse
import json
import sqlite3
import sys
import time

SCHEMA = """
CREATE TABLE IF NOT EXISTS readings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts REAL NOT NULL,
    station TEXT NOT NULL,
    temp_c REAL,
    humidity REAL,
    pressure_hpa REAL,
    dew_point_c REAL,
    rain TEXT,
    rain_raw INTEGER,
    light_pct INTEGER,
    uptime_s INTEGER
)
"""

FIELDS = ("temp_c", "humidity", "pressure_hpa", "dew_point_c",
          "rain", "rain_raw", "light_pct", "uptime_s")


def open_db(path):
    conn = sqlite3.connect(path)
    conn.execute(SCHEMA)
    conn.commit()
    return conn


def parse_payload(payload):
    """Return a dict for a valid reading, None for anything else."""
    if isinstance(payload, bytes):
        try:
            payload = payload.decode("utf-8")
        except UnicodeDecodeError:
            return None
    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        return None
    if not isinstance(data, dict) or "id" not in data:
        return None
    return data


def store(conn, data, ts=None):
    if ts is None:
        ts = data.get("ts", time.time())
    row = [ts, data["id"]] + [data.get(f) for f in FIELDS]
    conn.execute(
        "INSERT INTO readings (ts, station, %s) VALUES (?, ?, %s)"
        % (", ".join(FIELDS), ", ".join("?" * len(FIELDS))),
        row,
    )
    conn.commit()


def run_stdin(conn):
    count = 0
    bad = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        data = parse_payload(line)
        if data is None:
            bad += 1
            continue
        store(conn, data)
        count += 1
    print("stored %d readings, skipped %d bad lines" % (count, bad))


def run_mqtt(conn, host, port):
    import paho.mqtt.client as mqtt

    def on_connect(client, userdata, flags, rc, *args):
        print("connected to %s:%d rc=%s" % (host, port, rc))
        client.subscribe("weather/+/data")

    def on_message(client, userdata, msg):
        data = parse_payload(msg.payload)
        if data is None:
            print("bad payload on %s: %r" % (msg.topic, msg.payload[:80]))
            return
        store(conn, data)
        print("%s %s" % (msg.topic, msg.payload.decode()))

    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    except AttributeError:  # paho-mqtt 1.x
        client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(host, port, keepalive=60)
    client.loop_forever()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="weather.db")
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--mqtt", metavar="HOST")
    src.add_argument("--stdin", action="store_true")
    ap.add_argument("--port", type=int, default=1883)
    args = ap.parse_args()

    conn = open_db(args.db)
    if args.stdin:
        run_stdin(conn)
    else:
        run_mqtt(conn, args.mqtt, args.port)


if __name__ == "__main__":
    main()
