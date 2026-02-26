from typing import cast
import docker
import asyncio
import socket
from socket import SocketIO
import time

async def main():
    docker_client = docker.from_env()

    container = docker_client.containers.run(
        "alpine:3.21", command="sleep infinity", detach=True
    )
    print("Container created")
    print(container.id)
    exec_instance = docker_client.api.exec_create(
        container=container.id,
        cmd=["/bin/sh"],
        stdin=True,
        stdout=True,
        stderr=False,
        tty=True,
    )

    socket = cast(SocketIO, docker_client.api.exec_start(
        exec_instance["Id"],
        detach=False,
        socket=True, 
    ))


    # # Write command
    # socket.write("echo hello\n".encode("utf-8"))
    # socket.flush()

    # time.sleep(0.2)

    # # Read response
    # output = socket.read(4096)
    # print(output.decode())

    socket.close()

if __name__ == "__main__":
    asyncio.run(main())
