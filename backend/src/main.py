from typing import cast
from socket import SocketIO
import socketio
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from collections import defaultdict
import logging
import asyncio
import uvicorn

logging.basicConfig(
    level=logging.DEBUG,
    format="%(levelname)s | %(asctime)s  | "
    "%(module)s:%(funcName)s:%(lineno)d - %(message)s",
)

app = FastAPI()
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173"],      
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

sio = socketio.AsyncServer(cors_allowed_origins=["http://localhost:5173"], async_mode="asgi")
socket_app = socketio.ASGIApp(sio, app)

@sio.event
async def create_session(sid, data):
    logging.info("Received websocket connection")
    await sio.emit("response_event", f"Server received: {data}", room=sid)

if __name__ == "__main__":
    uvicorn.run(socket_app, host="0.0.0.0", port=8000, log_level="info")
