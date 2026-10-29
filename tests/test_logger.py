import json
import os
import subprocess

import pytest

import sim_station
import wx_logger

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WX_JSON = os.path.join(ROOT, "build", "wx_json")


@pytest.fixture
def db(tmp_path):
    conn = wx_logger.open_db(str(tmp_path / "test.db"))
    yield conn
    conn.close()


def test_parse_good():
    d = wx_logger.parse_payload(b'{"id":"wx01","temp_c":21.5}')
    assert d["id"] == "wx01"
    assert d["temp_c"] == 21.5


@pytest.mark.parametrize("payload", [
    b"", b"not json", b"[1,2,3]", b'{"temp_c":1}', b"\xff\xfe",
])
def test_parse_bad(payload):
    assert wx_logger.parse_payload(payload) is None


def test_store_and_read_back(db):
    wx_logger.store(db, {"id": "wx01", "temp_c": 22.0, "humidity": None,
                         "rain": "dry"}, ts=1000.0)
    row = db.execute("SELECT ts, station, temp_c, humidity, rain "
                     "FROM readings").fetchone()
    assert row == (1000.0, "wx01", 22.0, None, "dry")


@pytest.mark.skipif(not os.path.exists(WX_JSON), reason="run make first")
def test_logger_accepts_firmware_json(db):
    # same formatting code that runs on the ESP32
    out = subprocess.run([WX_JSON], input="24.3 55 1008.2 4000 2048 120\n"
                         "nan nan nan 900 10 150\n",
                         capture_output=True, text=True, check=True).stdout
    lines = out.strip().splitlines()
    assert len(lines) == 2
    for line in lines:
        d = wx_logger.parse_payload(line)
        assert d is not None
        wx_logger.store(db, d)

    rows = db.execute("SELECT temp_c, humidity, rain FROM readings "
                      "ORDER BY id").fetchall()
    assert rows[0] == (24.3, 55.0, "dry")
    assert rows[1] == (None, None, "heavy")


def test_sim_rain_shower_in_afternoon():
    import random
    rng = random.Random(1)
    r_morning = sim_station.make_reading(9 * 3600, 0, rng)
    r_shower = sim_station.make_reading(16.5 * 3600, 0, rng)
    assert r_morning["rain"] == "dry"
    assert r_shower["rain"] in ("light", "heavy")


def test_sim_matches_c_dew_point():
    if not os.path.exists(WX_JSON):
        pytest.skip("run make first")
    out = subprocess.run([WX_JSON], input="25.0 60 1000 4000 0 0\n",
                         capture_output=True, text=True, check=True).stdout
    c_dew = json.loads(out)["dew_point_c"]
    assert abs(sim_station.dew_point(25.0, 60.0) - c_dew) < 0.1
