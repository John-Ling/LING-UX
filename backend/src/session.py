from fastapi import WebSocket
from docker.models.containers import Container
import docker


class Session:
    def __init__(self, id, socket: WebSocket):
        self.id = id
        docker_client = docker.from_env()
        self.shell: Container = docker_client.containers.run("alpine:3.21", detach=True)
        self.socket = socket
        docker_client.close()
