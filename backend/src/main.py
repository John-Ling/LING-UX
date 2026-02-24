from fastapi import FastAPI, WebSocket
from fastapi.responses import JSONResponse
from collections import defaultdict
import logging
import asyncio
import uvicorn

logging.basicConfig(
    level=logging.DEBUG,
    format="%(levelname)s | %(asctime)s  | "
    "%(module)s:%(funcName)s:%(lineno)d - %(message)s",
)

active_sessions: defaultdict[str, set[WebSocket]] = defaultdict(set)
app = FastAPI()


@app.websocket("/session/{session_id}")
async def connect_session(session_id: str, socket: WebSocket):
    try:
        await socket.accept()
        active_sessions[session_id].add(socket)
        while True:
            logging.info("Sending bytes")
            await socket.send_text("Hello World")
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        logging.info("Stopping")
        raise KeyboardInterrupt
    except Exception as e:
        logging.error(f"[ERROR] connect_session socket failed: {e}")
    finally:
        logging.info("[INFO] Closing session")
        active_sessions[session_id].discard(socket)
        await socket.close()
        
@app.get("/sessions")
async def get_sessions():
    return {"sessions": list(active_sessions.keys())}


if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, log_level="info")
