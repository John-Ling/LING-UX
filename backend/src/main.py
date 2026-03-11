import socketio
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from collections import defaultdict
import asyncio
import uvicorn
import docker

from models.session import Session
from routes.socketio import sockets
from logger import logger

# Only one session should exist per session id
sessions: defaultdict[str, set[Session]] = defaultdict(set)

allowed_origins = [
    "http://localhost:5173",
    "https://terminal.johnling.me",
    "http://127.0.0.1",
]

sio = socketio.AsyncServer(
    cors_allowed_origins=allowed_origins,
    async_mode="asgi",
)

app = FastAPI(root_path="/api")
app.add_middleware(
    CORSMiddleware,
    allow_origins=allowed_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

socket_app = socketio.ASGIApp(sio, socketio_path="/api/socket.io", other_asgi_app=app)
docker_client = docker.from_env()
sockets.register_handlers(sio, sessions, docker_client)


@app.get("/sessions")
async def get_sessions():
    logger.info("Getting sessions")

    return {"sessions": list(sessions.keys())}

@app.get("/debug/ping/{sid}")
async def debug_ping(sid: str):
    session_set = sessions.get(sid)
    if not session_set:
        return {"error": "no session"}
    
    session = next(iter(session_set))
    loop = asyncio.get_running_loop()
    
    # Check socket is alive
    fd = session.raw_socket.fileno()
    
    # Write directly
    await loop.run_in_executor(None, lambda: session.raw_socket.sendall(b"echo ping123\n"))
    await asyncio.sleep(0.3)
    
    # Read directly, bypassing select
    try:
        data = await loop.run_in_executor(None, lambda: session.raw_socket.recv(4096))
        return {"fd": fd, "response": data.decode(errors="ignore"), "bytes": len(data)}
    except BlockingIOError:
        return {"fd": fd, "response": "no data available (BlockingIOError)"}
    except Exception as e:
        return {"fd": fd, "error": str(e)}

if __name__ == "__main__":
    uvicorn.run(socket_app, host="0.0.0.0", port=8000)
