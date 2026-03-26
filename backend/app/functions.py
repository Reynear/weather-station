from urllib.parse import quote_plus

from flask import Flask
from pymongo import MongoClient


class DB:
    def __init__(self):
        self.client = None
        self.database = None
        self.collection = None

    def init_app(self, app):
        username = quote_plus(app.config["DB_USERNAME"])
        password = quote_plus(app.config["DB_PASSWORD"])
        server = app.config["DB_SERVER"]
        port = app.config["DB_PORT"]
        uri = f"mongodb://{username}:{password}@{server}:{port}/"

        self.client = MongoClient(uri)
        self.database = self.client[app.config["MONGO_DATABASE"]]
        self.collection = self.database[app.config["MONGO_COLLECTION"]]

    def get_collection(self):
        if self.collection is None:
            raise RuntimeError("MongoDB collection is not initialized.")
        return self.collection
