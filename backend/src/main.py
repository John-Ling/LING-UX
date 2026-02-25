from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from collections import defaultdict
import logging
import asyncio
import uvicorn

logging.basicConfig(
    level=logging.DEBUG,
    format="%(levelname)s | %(asctime)s  | "
    "%(module)s:%(funcName)s:%(lineno)d - %(message)s",
)

# Todo, add a class for session to track attributes such as time, ttl
# and potentially a secret key to prevent users from visiting other's sessions
active_sessions: defaultdict[str, set[WebSocket]] = defaultdict(set)
app = FastAPI()

@app.websocket("/session/{session_id}")
async def connect_session(session_id: str, socket: WebSocket):
    session_active = True
    try:
        await socket.accept()
        active_sessions[session_id].add(socket)
        while session_active:
            logging.info("Sending bytes")
            await socket.send_text("Hello World\r\n")
            await asyncio.sleep(1)
    except WebSocketDisconnect:
        session_active = False
        logging.info("[INFO] client disconnected")
    finally:
        logging.info("[INFO] Closing session")
        active_sessions[session_id].discard(socket)
        if active_sessions[session_id] == set():
            del active_sessions[session_id]

@app.get("/sessions")
async def get_sessions():
    return {"sessions": list(active_sessions.keys())}


if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, log_level="info")
