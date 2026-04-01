#!/usr/bin/env python3
import argparse
import math
import random
import struct
from datetime import datetime, timedelta, timezone

from flask import Flask
from pymongo import InsertOne
from bson import ObjectId

from app.config import Config
from app.functions import DB


SEED_SOURCE = "historical_seed_v1"


def parse_date(value):
    return datetime.strptime(value, "%Y-%m-%d").replace(tzinfo=timezone.utc)


def clamp(value, low, high):
    return max(low, min(high, value))


def object_id_for_datetime(dt, counter):
    timestamp_bytes = struct.pack(">I", int(dt.timestamp()))
    suffix_bytes = counter.to_bytes(8, "big", signed=False)
    return ObjectId(timestamp_bytes + suffix_bytes)


def heat_index_celsius(temp_c, humidity):
    temp_f = (temp_c * 9 / 5) + 32
    hi_f = (
        -42.379
        + 2.04901523 * temp_f
        + 10.14333127 * humidity
        - 0.22475541 * temp_f * humidity
        - 0.00683783 * temp_f * temp_f
        - 0.05481717 * humidity * humidity
        + 0.00122874 * temp_f * temp_f * humidity
        + 0.00085282 * temp_f * humidity * humidity
        - 0.00000199 * temp_f * temp_f * humidity * humidity
    )
    hi_c = (hi_f - 32) * 5 / 9
    return max(temp_c, hi_c)


def build_documents(start_date, end_date, interval_minutes, seed):
    rng = random.Random(seed)
    total_days = max((end_date - start_date).days + 1, 1)
    docs = []
    timestamp = start_date
    end_boundary = end_date + timedelta(days=1) - timedelta(minutes=interval_minutes)
    counter = 1

    while timestamp <= end_boundary:
        minutes_since_midnight = timestamp.hour * 60 + timestamp.minute
        day_fraction = minutes_since_midnight / (24 * 60)
        season_fraction = ((timestamp.date() - start_date.date()).days) / total_days

        morning_warmup = math.sin((day_fraction - 0.22) * 2 * math.pi)
        humidity_cycle = math.cos((day_fraction - 0.1) * 2 * math.pi)
        pressure_cycle = math.sin((day_fraction + 0.15) * 2 * math.pi)
        seasonal_shift = math.sin(season_fraction * 2 * math.pi)

        temperature = 27.5 + (3.8 * morning_warmup) + (0.8 * seasonal_shift) + rng.uniform(-0.6, 0.6)
        humidity = 73 - (11 * morning_warmup) + (4 * humidity_cycle) + rng.uniform(-2.2, 2.2)
        pressure = 993.2 + (1.2 * pressure_cycle) + (0.9 * seasonal_shift) + rng.uniform(-0.35, 0.35)

        rainfall_pulse = 0
        if rng.random() < 0.05:
            rainfall_pulse = rng.uniform(4, 14)

        previous_soil = docs[-1]["soil_moisture"] if docs else 10.0
        soil_moisture = previous_soil * 0.992 + rainfall_pulse * 0.45 + rng.uniform(-0.35, 0.35)
        altitude = 156.0 + rng.uniform(-2.5, 2.5) + 0.15 * pressure_cycle

        temperature = round(clamp(temperature, 22.5, 34.8), 1)
        humidity = round(clamp(humidity, 48.0, 94.0), 1)
        pressure = round(clamp(pressure, 989.5, 1000.5), 1)
        soil_moisture = round(clamp(soil_moisture, 0.0, 85.0), 1)
        altitude = round(clamp(altitude, 145.0, 166.0), 1)
        heat_index = round(clamp(heat_index_celsius(temperature, humidity), temperature, 44.0), 1)

        docs.append(
            {
                "_id": object_id_for_datetime(timestamp, counter),
                "recorded_at": timestamp,
                "device_id": "620171008",
                "temperature": temperature,
                "humidity": humidity,
                "pressure": pressure,
                "soil_moisture": soil_moisture,
                "altitude": altitude,
                "heat_index": heat_index,
                "wifi_connected": True,
                "uptime_seconds": counter * interval_minutes * 60,
                "seed_source": SEED_SOURCE,
            }
        )

        timestamp += timedelta(minutes=interval_minutes)
        counter += 1

    return docs


def main():
    parser = argparse.ArgumentParser(description="Seed synthetic historical weather data.")
    parser.add_argument("--start", default="2026-03-01", help="Start date in YYYY-MM-DD")
    parser.add_argument("--end", default="2026-04-01", help="End date in YYYY-MM-DD")
    parser.add_argument("--interval-minutes", type=int, default=15, help="Minutes between readings")
    parser.add_argument("--seed", type=int, default=620171008, help="Random seed for deterministic output")
    parser.add_argument("--dry-run", action="store_true", help="Preview document count without writing")
    args = parser.parse_args()

    start_date = parse_date(args.start)
    end_date = parse_date(args.end)
    if start_date > end_date:
        raise SystemExit("start date must be earlier than or equal to end date")

    app = Flask(__name__)
    app.config.from_object(Config)
    db = DB()
    db.init_app(app)
    collection = db.get_collection()

    docs = build_documents(start_date, end_date, args.interval_minutes, args.seed)
    start_oid = ObjectId.from_datetime(start_date)
    end_oid = ObjectId.from_datetime(end_date + timedelta(days=1))

    if args.dry_run:
        print(f"Would insert {len(docs)} synthetic readings from {args.start} to {args.end}.")
        return

    deleted = collection.delete_many(
        {
            "seed_source": SEED_SOURCE,
            "_id": {"$gte": start_oid, "$lt": end_oid},
        }
    ).deleted_count

    operations = [InsertOne(doc) for doc in docs]
    result = collection.bulk_write(operations, ordered=False)

    print(
        f"Inserted {result.inserted_count} synthetic readings "
        f"from {args.start} to {args.end} at {args.interval_minutes}-minute intervals."
    )
    print(f"Removed {deleted} previous seeded readings in that range.")


if __name__ == "__main__":
    main()
