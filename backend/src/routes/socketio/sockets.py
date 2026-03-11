from socketio import AsyncServer
from collections import defaultdict
from docker import DockerClient
import asyncio
import select
import time

from logger import logger
from models.session import Session
import re

def register_handlers(
    sio: AsyncServer,
    sessions: defaultdict[str, set[Session]],
    docker_client: DockerClient,
):
    @sio.on("create_session")
    async def session(sid):
        logger.info("Received websocket connection")
        logger.info(f"Session id {sid}")

        session = Session(sid, docker_client)
        sessions[sid].add(session)
        

        await sio.emit("session_created", sid, room=sid)

    @sio.on("send-to-terminal")
    async def receive(sid: str, data: dict):
        """
        Receives data from a specific client and writes to the correct socket
        """
        session = get_session(sid)
        if session is None:
            raise RuntimeError("No session exists")

        logger.info(f"Received command {data["command"]}")
        logger.info(f"Writing to container socket")
        session.last_updated = time.time()
        await write_to_socket(session, data["command"].encode())
        logger.info("Socket written to successfully")

    @sio.on("resize-terminal")
    async def resize(sid: str, data: dict):
        logger.info("RESiZING")

        session = get_session(sid)
        if session is None:
            raise RuntimeError("No session exists")
        resize_session(session, data["row_count"], data["column_count"])

    def get_session(sid: str):
        session_set = sessions.get(sid, None)

        if session_set is None:
            logger.error(f"Session {sid} does not exist")
            return None

        session = next(iter(session_set))
        return session

    async def write_to_socket(session: Session, data: bytes):
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(
            executor=None, func=lambda: session.raw_socket.sendall(data)
        )

    def resize_session(session: Session, row_count: int, column_count: int):
        session.container.resize(height=row_count, width=column_count)

    async def read_and_send_to_client():
        """
        Read data from all sessions and send it through their correct sockets
        """

        loop = asyncio.get_running_loop()

        READ_SIZE = 4096  # typical size of a pty buffer

        def get_session_by_socket(socket) -> Session | None:
            return next(
                (
                    session
                    for session in _sessions
                    if session is not None and session.raw_socket is socket
                ),
                None,
            )
    


        while True:
            dead_sessions: list[Session] = []
            _sessions = [get_session(sid) for sid in sessions.keys()]
            _sessions = [session for session in _sessions if session is not None]

            sockets = [session.raw_socket for session in _sessions]

            # Check if any sockets are available to be read via select call
            # rlist = [sockets] monitor all sessions for reading
            # wlist = [] nothing to write
            # xlist = [] nothing to monitor for "exceptional conditions"
            (available_for_read, _, _) = await loop.run_in_executor(
                executor=None, func=lambda: select.select(sockets, [], [], 0.1)
            )

            for socket in available_for_read:
                logger.info(f"Reading data from socket")
                try:
                    session = get_session_by_socket(socket)
                    if session is None:
                        logger.error("Could not find session by socket")
                        dead_sessions.append(session)
                        continue

                    output = await loop.run_in_executor(
                        executor=None,
                        func=lambda s=socket: s.recv(READ_SIZE).decode(errors="ignore"),
                    )
                    if not output:
                        logger.info(f"Session {session.id} container exited")
                        await sio.emit("terminal-receive", {"output": "ERROR"}, to=session.id)
                        dead_sessions.append(session)
                        continue

                    logger.info(f"Sending to session {session.id}")
                    await sio.emit("terminal-receive", {"output": output}, to=session.id)
                except OSError as err:
                    # Ignore OS reading errors to keep the sessions going
                    logger.exception(f"Failure: {err}")

            await asyncio.sleep(0.05)  # Yield to event loop
            for session in dead_sessions:
                await loop.run_in_executor(executor=None, func=lambda: session.close())


    @sio.on("disconnect")
    async def disconnect(sid):
        loop = asyncio.get_running_loop()
        logger.info(f"Closing session {sid}")
        session = get_session(sid)
        if session:
            await loop.run_in_executor(executor=None, func=lambda: session.close())
            del sessions[sid]
            logger.info(f"Closing completed")
    
    logger.info("Starting read write loop")
    sio.start_background_task(read_and_send_to_client)
