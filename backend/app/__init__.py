from flask import Flask
from flask_cors import CORS

from .config import Config
from .functions import DB
from .mqtt import MQTT

mongo = DB()
mqtt = MQTT()


def create_app():
    app = Flask(__name__)
    CORS(app)
    app.config.from_object(Config)

    mongo.init_app(app)
    mqtt.init_app(app, mongo)

    from . import views
    app.register_blueprint(views.bp)

    return app
