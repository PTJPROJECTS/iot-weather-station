#!/usr/bin/env python3
"""Plot temperature, humidity and pressure from the SQLite log."""
import argparse
import datetime
import sqlite3

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(db, station):
    conn = sqlite3.connect(db)
    rows = conn.execute(
        "SELECT ts, temp_c, humidity, pressure_hpa, light_pct FROM readings "
        "WHERE station = ? ORDER BY ts", (station,)).fetchall()
    conn.close()
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="weather.db")
    ap.add_argument("--station", default="wx01")
    ap.add_argument("--out", default="weather.png")
    args = ap.parse_args()

    rows = load(args.db, args.station)
    if not rows:
        raise SystemExit("no data for station %s" % args.station)

    t = [datetime.datetime.fromtimestamp(r[0]) for r in rows]
    fig, ax = plt.subplots(4, 1, sharex=True, figsize=(10, 8))
    ax[0].plot(t, [r[1] for r in rows]); ax[0].set_ylabel("temp (C)")
    ax[1].plot(t, [r[2] for r in rows]); ax[1].set_ylabel("RH (%)")
    ax[2].plot(t, [r[3] for r in rows]); ax[2].set_ylabel("hPa")
    ax[3].plot(t, [r[4] for r in rows]); ax[3].set_ylabel("light (%)")
    for a in ax:
        a.grid(True, alpha=0.3)
    fig.suptitle("station %s, %d readings" % (args.station, len(rows)))
    fig.autofmt_xdate()
    fig.tight_layout()
    fig.savefig(args.out, dpi=100)
    print("wrote %s (%d readings)" % (args.out, len(rows)))


if __name__ == "__main__":
    main()
