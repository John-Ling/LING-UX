import { useEffect, useRef } from "react";

export const useSocketConnection = (
  onReceive: (ev: MessageEvent) => void,
  onOpen: (self: WebSocket, ev: Event) => any,
  onClose: (self: WebSocket, ev: CloseEvent) => void
) => {
  const socketRef = useRef<WebSocket | null>(null);

  // Establish websocket connection
  useEffect(() => {
    console.log("[LOG] connecting to websocket");
    const socket = new WebSocket("ws://localhost:8000/session/12345");
    if (!socket) {
      return;
    }

    socketRef.current = socket;

    socket.onmessage = ev => onReceive(ev);
    socket.onopen = ev => onOpen(socket, ev);
    socket.onclose = ev => onClose(socket, ev);

    return () => {
      socket.close();
    }
  }, []);

  const handle_tab_close = () => {
    const socket = socketRef.current;
    if (!socket) {
      return;
    }

    socket.close();
  }

  // Close socket when tab is closed
  useEffect(() => {
    window.addEventListener("beforeunload", handle_tab_close);
    return () => {
      window.removeEventListener("beforeunload", handle_tab_close);
    }
  }, []);
  return { socketRef };
}