from socketio import AsyncServer
from collections import defaultdict
import asyncio
import pty
import fcntl
import termios
import struct
import os
import select
import time

from logger import logger
from models.session import Session


def register_handlers(sio: AsyncServer, sessions: defaultdict[str, set[Session]]):
    @sio.on("create_session")
    async def session(sid):
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
        Receives data from a specific client and writes to the correct pty
        """
        session = get_session(sid)
        if session is None:
            raise RuntimeError("No session exists")

        logger.info(f"Received command {data["command"]}")
        logger.info(f"Writing to file descriptor {session.file_descriptor}")
        session.last_updated = time.time()
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

        loop = asyncio.get_running_loop()

        READ_SIZE = 4096  # typical size of a pty buffer

        while True:
            _sessions = [get_session(sid) for sid in sessions.keys()]
            _sessions = [session for session in _sessions if session is not None]
            file_descriptors = [session.file_descriptor for session in _sessions]
            file_descriptor_to_sessions = {
                session.file_descriptor: session for session in _sessions
            }

            # Check if file descriptor is available to be read via select call
            # rlist = [file_descriptors] monitor all sessions for reading
            # wlist = [] nothing to write
            # xlist = [] nothing to monitor for "exceptional conditions"
            (available_for_read, _, _) = await loop.run_in_executor(
                executor=None, func=lambda: select.select(file_descriptors, [], [], 0.1)
            )

            for descriptor in available_for_read:
                logger.info(f"Reading data from file descriptor {descriptor}")
                try:
                    output = await loop.run_in_executor(
                        executor=None,
                        func=lambda: os.read(descriptor, READ_SIZE).decode(
                            errors="ignore"
                        ),
                    )
                    target = file_descriptor_to_sessions[descriptor].id
                    logger.info(f"SENDING DATA {output} TO SESSION {target}")
                    await sio.emit("pty-receive", {"output": output}, to=target)
                except OSError as err:
                    # Ignore OS reading errors to keep the sessions going
                    logger.exception(f"Failure: {err}")

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
