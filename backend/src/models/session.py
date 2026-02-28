from typing import Any
from docker import DockerClient
from docker.models.containers import Container
import time


class Session:
    def __init__(self, id: int, docker_client: DockerClient):
        self.id = id
        self.last_updated = time.time()
        self.container: Container = docker_client.containers.run(
            "alpine:latest",
            command="/bin/sh",
            name=f"session_{id}",
            detach=True,
            tty=True,
            stdin_open=True,
            network_mode="none",
            mem_limit="128m",
            nano_cpus=500_000_000,
            cap_drop=["ALL"],
            security_opt=["no-new-privileges:true"],
            remove=False,
            tmpfs={"/home": "size=64m,mode=1777"},
            environment={
                "TERM": "dumb",
                "PS1": "$ ",
            },
        )
        self.socket: Any = self.container.attach_socket(
            params={"stdin": 1, "stdout": 1, "stderr": 1, "stream": 1}
        )
        self.raw_socket: Any = self.socket._sock

    def close(self):
        try:
            self.container.stop(timeout=5)
        except Exception:
            self.container.kill()
        self.container.remove()
