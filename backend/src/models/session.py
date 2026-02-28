import time

class Session:
    def __init__(self, id: str, file_descriptor: int):
        self.id = id
        self.file_descriptor = file_descriptor  # Used for terminal IO
        self.last_updated = time.time()
