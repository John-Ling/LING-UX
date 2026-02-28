from typing import cast
from socket import SocketIO
import socketio
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from collections import defaultdict

import asyncio
import pty
import logging
import fcntl
import termios
import struct
import os
import select
import sys
import uvicorn

from session import Session

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],  # Output logs to standard output
)

# Only one session should exist per session id
sessions: defaultdict[str, set[Session]] = defaultdict(set)

logger = logging.getLogger(__name__)

app = FastAPI()
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

sio = socketio.AsyncServer(
    cors_allowed_origins=["http://localhost:5173"], async_mode="asgi"
)
socket_app = socketio.ASGIApp(sio, other_asgi_app=app)


@sio.on("create_session")
async def session(sid, data):
    logger.info("Received websocket connection")
    logger.info(f"Session id {sid}")

    # Create new pseudo terminal
    (pid, file_descriptor) = pty.fork()

    session = Session(sid, file_descriptor)
    sessions[sid].add(session)

    if pid == 0:
        # Child process, run bash
        env = os.environ.copy()
        env["TERM"] = "dumb"
        env["PS1"] = "$ "
        env["ZDOTDIR"] = "/nonexistent"
        env.pop("ZSH", None)
        env.pop("ZSH_THEME", None)
        os.execvpe("/bin/bash", ["/bin/bash", "--noprofile", "--norc"], env)
    else:
        # There should be a single parent process that handles reading and writing to all clients
        logger.info("Starting read write loop")
        sio.start_background_task(read_and_send_to_client)

    await sio.emit("session_created", sid, room=sid)


@sio.on("send-to-terminal")
async def receive(sid: str, data: dict):
    """
    Receives a command from a particular client, executes it in the pty and streams the output back
    until either command finishes or client issues CTRL-C or CTRL-D
    """
    session = get_session(sid)
    if session is None:
        raise RuntimeError("No session exists")
    
    logger.info(f"Received command {data["command"]}")
    logger.info(f"Writing to file descriptor {session.file_descriptor}")
    write_to_pty(session.file_descriptor, data["command"])


@sio.on("resize-terminal")
async def resize(sid: str, data: dict):
    logger.info("RESiZING")
    session = get_session(sid)
    if session is None:
        raise RuntimeError("No session exists")
    resize_pty(session.file_descriptor, data["row_count"], data["column_count"])


def get_session(sid: str):
    session_set = sessions.get(sid, None)

    if session_set is None:
        logger.error(f"Session {sid} does not exist")
        return None

    session = next(iter(session_set))
    return session


def write_to_pty(file_descriptor: int, data: str):
    os.write(file_descriptor, data.encode())


async def read_and_send_to_client():
    """
    Read data from all sessions and send it through their correct sockets
    """

    loop = asyncio.get_event_loop()

    READ_SIZE = 4096  # typical size of a pty buffer
    
    while True:
        _sessions = [get_session(sid) for sid in sessions.keys()]
        _sessions = [session for session in _sessions if session is not None]
        file_descriptors = [session.file_descriptor for session in _sessions]
        file_descriptor_to_sessions = {session.file_descriptor: session for session in _sessions}

        # Check if file descriptor is available to be read via select call
        # rlist = [file_descriptors] monitor all sessions for reading
        # wlist = [] nothing to write
        # xlist = [] nothing to monitor for "exceptional conditions"
        # timeout is set so we don't block
        (available_for_read, _, _) = await loop.run_in_executor(
            None, lambda: select.select(file_descriptors, [], [], 0.1)
        )
        for descriptor in available_for_read:
            logger.info(f"Reading data from file descriptor {descriptor}")
            try:
                output = os.read(descriptor, READ_SIZE).decode(errors="ignore")
                target = file_descriptor_to_sessions[descriptor].id
                logger.info(f"SENDING DATA {output} TO SESSION {target}")
                await sio.emit("pty-receive", {"output": output}, to=target)
            except OSError as err:
                logger.error(f"Failure: {err}")

        await asyncio.sleep(0.05)  # Yield to event loop


def resize_pty(file_descriptor: int, row_count, column_count):
    winsize = struct.pack("HH", row_count, column_count)
    fcntl.ioctl(file_descriptor, termios.TIOCSWINSZ, winsize)


async def send(sid, data):
    """
    Sends data back the client
    """


@sio.event
def connect(sid, environ):
    print("I'm connected!")


@sio.event
def connect_error(data):
    print("The connection failed!")


@sio.event
def disconnect(sid):
    if sid in sessions:
        del sessions[sid]

@app.get("/sessions")
async def get_sessions():
    logging.info("Getting sessions")

    return {"sessions": list(sessions.keys())}


if __name__ == "__main__":
    uvicorn.run(socket_app, host="0.0.0.0", port=8000)
