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
        await asyncio.sleep(0.15)
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(None, lambda: session.raw_socket.sendall(b"\n"))

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
        READ_SIZE = 4096

        def get_session_by_socket(socket) -> Session | None:
            return next(
                (s for s in _sessions if s is not None and s.raw_socket is socket),
                None,
            )

        while True:
            try:
                current_sids = list(sessions.keys())
                _sessions = [get_session(sid) for sid in current_sids]
                _sessions = [s for s in _sessions if s is not None]
                sockets = [s.raw_socket for s in _sessions]

                if not sockets:
                    await asyncio.sleep(0.1)
                    continue

                (readable, _, _) = await loop.run_in_executor(
                    executor=None, func=lambda: select.select(sockets, [], [], 0.05)
                )

                if readable:
                    logger.info(f"select returned {len(readable)} readable sockets")

                for socket in readable:
                    try:
                        session = get_session_by_socket(socket)
                        if session is None:
                            continue

                        output = await loop.run_in_executor(
                            executor=None, func=lambda s=socket: s.recv(READ_SIZE).decode(errors="ignore")
                        )
                        if not output:
                            await sio.emit("session_ended", {}, to=session.id)
                            continue

                        await sio.emit("terminal-receive", {"output": output}, to=session.id)
                    except OSError as err:
                        logger.exception(f"Socket read failure: {err}")

                await asyncio.sleep(0.01)

            except Exception as e:
                # ✅ Log but never let the task die
                logger.exception(f"read_and_send_to_client loop error: {e}")
                await asyncio.sleep(0.1)  # brief pause before retrying

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
    
    
