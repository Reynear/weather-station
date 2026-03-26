import os

from dotenv import load_dotenv

load_dotenv()


class Config:
    DB_USERNAME = os.getenv("DB_USERNAME", "eletAdmin")
    DB_PASSWORD = os.getenv("DB_PASSWORD", "iUUodou54Ue7725QfDcSkAnT6G95NL62cCpZ")
    DB_SERVER = os.getenv("DB_SERVER", "localhost")
    DB_PORT = int(os.getenv("DB_PORT", "27017"))

    FLASK_RUN_HOST = os.getenv("FLASK_RUN_HOST", "127.0.0.1")
    FLASK_RUN_PORT = int(os.getenv("FLASK_RUN_PORT", "5000"))

    MONGO_DATABASE = "ELET2415"
    MONGO_COLLECTION = "weather_station"

    MQTT_BROKER_HOST = "www.yanacreations.com"
    MQTT_BROKER_PORT = 1883
    MQTT_MAIN_TOPIC = "620171008"
    MQTT_SUBSCRIBE_TOPIC = "620171008_sub"
    MQTT_PUBLISH_TOPIC = "620171008_pub"
    MQTT_CLIENT_ID = "620171008"
