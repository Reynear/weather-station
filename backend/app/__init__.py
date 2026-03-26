from flask import Flask

from .config import Config
from .functions import DB
from .mqtt import MQTT

mongo = DB()
mqtt = MQTT()


def create_app():
    app = Flask(__name__)
    app.config.from_object(Config)

    mongo.init_app(app)
    mqtt.init_app(app, mongo)

    return app
