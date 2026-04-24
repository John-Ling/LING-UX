from socketio import AsyncServer
from collections import defaultdict
from docker import DockerClient
from concurrent.futures import ThreadPoolExecutor
import asyncio
import time

from logger import logger
from models.session import Session

_executor = ThreadPoolExecutor(max_workers=10, thread_name_prefix="lingux-session")

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
        logger.info("Starting read write loop")
        sio.start_background_task(read_session, session) 


    @sio.on("send-to-terminal")
    async def receive(sid: str, data: dict):
        """
        Receives data from a specific client and writes to the correct socket
        """
        session = await get_session(sid)
        if session is None:
            return

        logger.info(f"Received command {data["command"]}")
        logger.info(f"Writing to container socket")
        session.last_updated = time.time()
        await write_to_socket(session, data["command"].encode())
        logger.info("Socket written to successfully")

    @sio.on("resize-terminal")
    async def resize(sid: str, data: dict):
        logger.info("RESiZING")

        session = await get_session(sid)
        if session is None:
            return
        
        resize_session(session, data["row_count"], data["column_count"])

    async def get_session(sid: str):
        session_set = sessions.get(sid, None)

        if session_set is None:
            logger.error(f"Session {sid} does not exist")
            await sio.emit("session_error", {"message": "Session not found"}, to=sid)
            return None

        session = next(iter(session_set))
        return session

    async def write_to_socket(session: Session, data: bytes):
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(
            executor=_executor, func=lambda: session.raw_socket.sendall(data)
        )

    def resize_session(session: Session, row_count: int, column_count: int):
        session.container.resize(height=row_count, width=column_count)

    async def read_session(session: Session):
        """
        IO loop for session. Read data from socket socket and write via websocket
        """
        READ_SIZE = 4096
        loop = asyncio.get_running_loop()
        logger.info(f"Starting reader for session {session.id}")

        try:
            fd = session.raw_socket.fileno()
            logger.info(f"Session {session.id} socket fd={fd}")
        except Exception as e:
            logger.error(f"Session {session.id} has no valid fd: {e}")
            return

        while True:
            try:
                output = await loop.run_in_executor(
                    _executor,
                    lambda: session.raw_socket.recv(READ_SIZE)
                )
                
                if not output:
                    logger.info(f"Session {session.id} socket closed")
                    break

                decoded = output.decode(errors="ignore")
                logger.info(f"Session {session.id} recv {len(output)} bytes: {repr(decoded[:50])}")
                await sio.emit("terminal-receive", {"output": decoded}, to=session.id)

            except OSError as e:
                logger.exception(f"Session {session.id} read error: {e}")
                break
            except Exception as e:
                logger.exception(f"Session {session.id} unexpected error: {e}")
                break

        logger.info(f"Reader for session {session.id} exiting")

    @sio.on("disconnect")
    async def disconnect(sid):
        loop = asyncio.get_running_loop()
        logger.info(f"Closing session {sid}")
        session = await get_session(sid)
        if session:
            await loop.run_in_executor(executor=_executor, func=lambda: session.close())
            del sessions[sid]
            logger.info(f"Closing completed")
