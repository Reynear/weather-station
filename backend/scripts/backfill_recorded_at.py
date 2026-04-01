#!/usr/bin/env python3
import argparse

from flask import Flask
from pymongo import UpdateOne

from app.config import Config
from app.functions import DB


def batched(iterable, size):
    batch = []
    for item in iterable:
        batch.append(item)
        if len(batch) >= size:
            yield batch
            batch = []
    if batch:
        yield batch


def build_app():
    app = Flask(__name__)
    app.config.from_object(Config)
    return app


def main():
    parser = argparse.ArgumentParser(description="Backfill recorded_at for existing weather readings.")
    parser.add_argument("--batch-size", type=int, default=1000, help="Number of updates to send per batch")
    parser.add_argument("--dry-run", action="store_true", help="Report how many documents need backfilling")
    args = parser.parse_args()

    app = build_app()
    db = DB()
    db.init_app(app)
    collection = db.get_collection()

    missing_query = {
        "$or": [
            {"recorded_at": {"$exists": False}},
            {"recorded_at": None},
        ]
    }
    missing_count = collection.count_documents(missing_query)

    if args.dry_run:
        print(f"Would backfill recorded_at on {missing_count} documents.")
        return

    modified_count = 0
    cursor = collection.find(missing_query, {"_id": 1}).sort("_id", 1)
    for batch in batched(cursor, args.batch_size):
        operations = [
            UpdateOne(
                {"_id": document["_id"]},
                {"$set": {"recorded_at": document["_id"].generation_time}},
            )
            for document in batch
        ]
        if not operations:
            continue

        result = collection.bulk_write(operations, ordered=False)
        modified_count += result.modified_count

    index_name = db.ensure_recorded_at_index()

    print(f"Backfilled recorded_at on {modified_count} documents.")
    print(f"Documents missing recorded_at before migration: {missing_count}.")
    print(f"Ensured MongoDB index: {index_name}.")


if __name__ == "__main__":
    main()
