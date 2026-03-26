import json

from flask import Flask
from paho.mqtt import client as mqtt


class MQTT:

    def __init__(self):
        self.app = None
        self.db = None
        self.client = None

    def init_app(self, app, db):
        self.app = app
        self.db = db

        client = mqtt.Client(
            client_id=app.config["MQTT_CLIENT_ID"],
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        )
        client.on_connect = self.on_connect
        client.on_message = self.on_message
        client.on_disconnect = self.on_disconnect
        self.client = client

        broker_host = app.config["MQTT_BROKER_HOST"]
        broker_port = app.config["MQTT_BROKER_PORT"]

        try:
            client.connect(broker_host, broker_port, keepalive=60)
            client.loop_start()
            print(
                f"Connecting to MQTT broker {broker_host}:{broker_port} "
                f"on topic {app.config['MQTT_SUBSCRIBE_TOPIC']}"
            )
        except Exception:
            print("Failed to connect to MQTT broker.")

    def on_connect(self, client, userdata, flags, reason_code, properties):
        if self.app is None:
            return

        if reason_code != 0:
            print(f"MQTT connection failed with reason code {reason_code}")
            return

        topic = self.app.config["MQTT_SUBSCRIBE_TOPIC"]
        client.subscribe(topic)
        print(f"Subscribed to MQTT topic {topic}")

    def on_disconnect(
        self, client, userdata, disconnect_flags, reason_code, properties
    ):
        if self.app is None:
            return
        print(f"MQTT disconnected with reason code {reason_code}")

    def on_message(self, client, userdata, message):
        if self.app is None or self.db is None:
            return

        raw_payload = message.payload.decode("utf-8", errors="replace")
        try:
            payload = json.loads(raw_payload)
            document = payload
            result = self.db.get_collection().insert_one(document)
            print(f"Inserted weather reading {result.inserted_id}")
        except Exception as exc:
            print(f"Failed to process MQTT payload: {exc}")

    def publish(self, payload):
        if self.app is None or self.client is None:
            return

        topic = self.app.config["MQTT_PUBLISH_TOPIC"]
        result = self.client.publish(topic, json.dumps(payload))
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            print(f"Failed to publish MQTT status to {topic}")
