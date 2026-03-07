import socketio
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from collections import defaultdict
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

app = FastAPI()
app.add_middleware(
    CORSMiddleware,
    allow_origins=allowed_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

socket_app = socketio.ASGIApp(sio, other_asgi_app=app)
docker_client = docker.from_env()
sockets.register_handlers(sio, sessions, docker_client)


@app.get("/sessions")
async def get_sessions():
    logger.info("Getting sessions")

    return {"sessions": list(sessions.keys())}


if __name__ == "__main__":
    uvicorn.run(socket_app, host="0.0.0.0", port=8000)
