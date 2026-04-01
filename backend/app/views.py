from datetime import datetime, timedelta, timezone

from bson.objectid import ObjectId
from flask import Blueprint, jsonify, request

from . import mongo

bp = Blueprint('api', __name__, url_prefix='/api')

ANALYSIS_METRICS = (
    "temperature",
    "humidity",
    "pressure",
    "soil_moisture",
    "altitude",
    "heat_index",
)


def parse_analysis_boundary(value, end_of_day=False):
    if "T" in value:
        parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
    else:
        parsed = datetime.strptime(value, "%Y-%m-%d").replace(tzinfo=timezone.utc)
        if end_of_day:
            parsed = parsed + timedelta(days=1) - timedelta(milliseconds=1)

    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def normalize_recorded_at(value):
    if isinstance(value, datetime):
        if value.tzinfo is None:
            return value.replace(tzinfo=timezone.utc)
        return value.astimezone(timezone.utc)
    if isinstance(value, str):
        parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
        if parsed.tzinfo is None:
            return parsed.replace(tzinfo=timezone.utc)
        return parsed.astimezone(timezone.utc)
    return None


def get_recorded_at(reading):
    recorded_at = normalize_recorded_at(reading.get("recorded_at"))
    if recorded_at is not None:
        return recorded_at
    return ObjectId(reading["_id"]).generation_time


def serialize_reading(reading):
    serialized = {**reading}
    recorded_at = get_recorded_at(reading)
    serialized["_id"] = str(reading["_id"])
    serialized["timestamp"] = recorded_at.isoformat()
    serialized.pop("recorded_at", None)
    return serialized


def build_time_range_query(start_date, end_date):
    legacy_range = {
        "$gte": ObjectId.from_datetime(start_date),
        "$lte": ObjectId.from_datetime(end_date),
    }

    return {
        "$or": [
            {
                "recorded_at": {
                    "$gte": start_date,
                    "$lte": end_date,
                }
            },
            {
                "recorded_at": {"$exists": False},
                "_id": legacy_range,
            },
            {
                "recorded_at": None,
                "_id": legacy_range,
            },
        ]
    }


def serialize_datetime(value):
    normalized = normalize_recorded_at(value)
    return normalized.isoformat() if normalized is not None else None


def round_metric_value(value):
    if value is None:
        return None
    return round(float(value), 1)


def empty_metric_aggregate():
    return {
        "count": 0,
        "min": None,
        "max": None,
        "avg": None,
        "latest": None,
    }


def serialize_aggregate_point(point):
    if not point:
        return None

    value = point.get("value")
    timestamp = point.get("timestamp")
    if value is None or timestamp is None:
        return None

    return {
        "value": round_metric_value(value),
        "timestamp": serialize_datetime(timestamp),
    }


def build_metric_aggregate_base_pipeline(metric, start_date, end_date):
    query = build_time_range_query(start_date, end_date)
    return [
        {"$match": query},
        {
            "$addFields": {
                "_resolved_timestamp": {
                    "$cond": [
                        {"$eq": [{"$type": "$recorded_at"}, "date"]},
                        "$recorded_at",
                        {"$toDate": "$_id"},
                    ]
                },
                "_metric_value": {
                    "$convert": {
                        "input": f"${metric}",
                        "to": "double",
                        "onError": None,
                        "onNull": None,
                    }
                },
            }
        },
        {"$match": {"_metric_value": {"$ne": None}}},
    ]


def run_metric_aggregate(collection, metric, start_date, end_date):
    base_pipeline = build_metric_aggregate_base_pipeline(metric, start_date, end_date)

    summary = next(collection.aggregate(base_pipeline + [
        {
            "$group": {
                "_id": None,
                "count": {"$sum": 1},
                "avg": {"$avg": "$_metric_value"},
            }
        }
    ]), None)

    count = int((summary or {}).get("count", 0) or 0)
    if count == 0:
        return empty_metric_aggregate()

    min_point = next(collection.aggregate(base_pipeline + [
        {"$sort": {"_metric_value": 1, "_resolved_timestamp": 1, "_id": 1}},
        {"$limit": 1},
        {
            "$project": {
                "_id": 0,
                "value": "$_metric_value",
                "timestamp": "$_resolved_timestamp",
            }
        },
    ]), None)

    max_point = next(collection.aggregate(base_pipeline + [
        {"$sort": {"_metric_value": -1, "_resolved_timestamp": 1, "_id": 1}},
        {"$limit": 1},
        {
            "$project": {
                "_id": 0,
                "value": "$_metric_value",
                "timestamp": "$_resolved_timestamp",
            }
        },
    ]), None)

    latest_point = next(collection.aggregate(base_pipeline + [
        {"$sort": {"_resolved_timestamp": -1, "_id": -1}},
        {"$limit": 1},
        {
            "$project": {
                "_id": 0,
                "value": "$_metric_value",
                "timestamp": "$_resolved_timestamp",
            }
        },
    ]), None)

    return {
        "count": count,
        "min": serialize_aggregate_point(min_point),
        "max": serialize_aggregate_point(max_point),
        "avg": round_metric_value(summary.get("avg")),
        "latest": serialize_aggregate_point(latest_point),
    }


@bp.route('/analysis', methods=['GET'])
def get_analysis_data():
    collection = mongo.get_collection()
    start_time_str = request.args.get('start_time')
    end_time_str = request.args.get('end_time')

    if not start_time_str or not end_time_str:
        return jsonify({
            "error": "start_time and end_time are required."
        }), 400

    try:
        start_date = parse_analysis_boundary(start_time_str)
        end_date = parse_analysis_boundary(end_time_str, end_of_day=True)
    except Exception:
        return jsonify({
            "error": "start_time and end_time must be valid ISO datetimes or YYYY-MM-DD dates."
        }), 400

    if start_date > end_date:
        return jsonify({
            "error": "start_time must be earlier than or equal to end_time."
        }), 400

    query = build_time_range_query(start_date, end_date)
    readings = sorted(collection.find(query), key=get_recorded_at)
    readings = [serialize_reading(reading) for reading in readings]
    return jsonify(readings)


@bp.route('/analysis/aggregates', methods=['GET'])
def get_analysis_aggregates():
    collection = mongo.get_collection()
    start_time_str = request.args.get('start_time')
    end_time_str = request.args.get('end_time')

    if not start_time_str or not end_time_str:
        return jsonify({
            "error": "start_time and end_time are required."
        }), 400

    try:
        start_date = parse_analysis_boundary(start_time_str)
        end_date = parse_analysis_boundary(end_time_str, end_of_day=True)
    except Exception:
        return jsonify({
            "error": "start_time and end_time must be valid ISO datetimes or YYYY-MM-DD dates."
        }), 400

    if start_date > end_date:
        return jsonify({
            "error": "start_time must be earlier than or equal to end_time."
        }), 400

    metrics = {
        metric: run_metric_aggregate(collection, metric, start_date, end_date)
        for metric in ANALYSIS_METRICS
    }

    return jsonify({
        "range": {
            "start_time": start_date.isoformat().replace("+00:00", "Z"),
            "end_time": end_date.isoformat().replace("+00:00", "Z"),
        },
        "metrics": metrics,
    })


@bp.route('/summary/today', methods=['GET'])
def get_today_summary():
    collection = mongo.get_collection()

    today = datetime.now(timezone.utc)
    start_of_today = datetime(today.year, today.month, today.day, tzinfo=timezone.utc)

    query = build_time_range_query(start_of_today, today)
    readings = sorted(collection.find(query), key=get_recorded_at)

    if not readings:
        return jsonify({"message": "No readings today yet."})
    
    # Calculate some basics
    temps = [r.get('temperature', 0) for r in readings if r.get('temperature') is not None]
    hums = [r.get('humidity', 0) for r in readings if r.get('humidity') is not None]
    
    summary = {
        "count": len(readings),
        "latest": serialize_reading(readings[-1]),
        "temperature": {
            "min": min(temps) if temps else 0,
            "max": max(temps) if temps else 0,
            "avg": sum(temps) / len(temps) if temps else 0
        },
        "humidity": {
            "min": min(hums) if hums else 0,
            "max": max(hums) if hums else 0,
            "avg": sum(hums) / len(hums) if hums else 0
        }
    }
    
    return jsonify(summary)
