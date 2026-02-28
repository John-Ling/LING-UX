from contextlib import asynccontextmanager

import socketio
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from apscheduler.schedulers.background import BackgroundScheduler
from apscheduler.triggers.cron import CronTrigger
from collections import defaultdict
from routes.socketio import pty 
import uvicorn

from session import Session
from logger import logger

# Only one session should exist per session id
sessions: defaultdict[str, set[Session]] = defaultdict(set)

scheduler = BackgroundScheduler()
trigger = CronTrigger(hour=0, minute=0) # Run at midnight daily

scheduler.start()

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield
    scheduler.shutdown()

sio = socketio.AsyncServer(
    cors_allowed_origins=["http://localhost:5173"], async_mode="asgi"
)

app = FastAPI(lifespan=lifespan)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

socket_app = socketio.ASGIApp(sio, other_asgi_app=app)
pty.register_handlers(sio, sessions)

@app.get("/sessions")
async def get_sessions():
    logger.info("Getting sessions")

    return {"sessions": list(sessions.keys())}

if __name__ == "__main__":
    uvicorn.run(socket_app, host="0.0.0.0", port=8000)
