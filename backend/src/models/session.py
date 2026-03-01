from typing import Any
from docker import DockerClient
from docker.models.containers import Container
import time
import glob

import tarfile
import io
import os


def copy_to_container(container, src_pattern: str, dest_path: str):
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w") as tar:
        for path in glob.glob(src_pattern, recursive=True):
            tar.add(path, arcname=os.path.basename(path))
    buf.seek(0)
    container.put_archive(dest_path, buf)


class Session:
    def __init__(self, id: int, docker_client: DockerClient):
        self.id = id
        self.last_updated = time.time()
        self.container: Container = docker_client.containers.run(
            "web-shell-user-container:latest",
            command=["/bin/bash", "--rcfile", "/etc/bash.bashrc"],
            working_dir="/home",
            name=f"session_{id}",
            detach=True,
            tty=True,
            stdin_open=True,
            network_mode="none",
            mem_limit="128m",
            user="1000:1000",  
            nano_cpus=500_000_000,
            cap_drop=["ALL"],
            security_opt=["no-new-privileges:true"],
            remove=False,
            tmpfs={"/tmp": "size=64m,mode=1777"},
            environment={
                "TERM": "vt100",
                "PS1": r"guest@workstation \w $ ",
            },
        )

        copy_to_container(
            self.container,
            "/home/john/Projects/web-shell-64/backend/home-content/*",
            "/home",
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
