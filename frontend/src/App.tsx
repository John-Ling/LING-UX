import { Terminal } from '@xterm/xterm';
import './App.css'
import { io } from 'socket.io-client';
import { useEffect, useRef, useState } from 'react'
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';
import type { DisconnectDescription, Socket } from 'socket.io-client';

function App() {
  const terminalRef = useRef<HTMLDivElement | null>(null);
  const terminalBufferRef = useRef<string>("");
  const xtermRef = useRef<Terminal | null>(null);
  const fitAddonRef = useRef<FitAddon | null>(null);

  const socketReceive = (data: any) => {
    console.log("Received data")
    const terminal = xtermRef.current;
    if (!terminal) {
      return;
    }

    console.log(data)
    console.log(data["output"])
    if (data["output"]) {
      terminal.write(data["output"]);
    }
  }

  const socketOpen = async (self: Socket) => {
    console.log("[LOG] Opening websocket connection");
    self.emit("create_session", "HelLo Backend");
  }

  const socketClose = (reason: Socket.DisconnectReason, description?: DisconnectDescription) => {
    console.log("[LOG] Closing websocket. Goodbye :)");
  }

  const { socketRef } = useSocketConnection(socketReceive, socketOpen, socketClose);

  const handleData = (arg1: string) => {
    console.log("DATA", arg1)
    const socket = socketRef.current;
    if (socket) {
      socket.emit("send-to-terminal", {"command": arg1})
    }
  }

  const initPrompt = () => {
    if (!xtermRef.current) {
      return;
    }

    const terminal = xtermRef.current;
    terminal.write("guest@workstation ~ ");

  }

  // Init xterm.js
  useEffect(() => {
    const terminal = new Terminal({
      fontFamily: "PixelCode",
      theme: {
        background: "#1c0902",
      }
    });

    const fitAddon = new FitAddon();
    terminal.loadAddon(fitAddon);

    if (!terminalRef.current) {
      return;
    }

    terminal.open(terminalRef.current);
    fitAddon.fit();

    xtermRef.current = terminal;
    fitAddonRef.current = fitAddon;

    initPrompt();
    // terminal.onKey(handleKeyDown);
    terminal.onData(handleData);

    const handleResize = () => fitAddonRef.current?.fit();
    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      terminal.dispose();
    };
  }, []);

  return (
    <div className="root">
      <div className="container">
        {/* <div className="terminal-overlay"/> */}
        <div className="vignette" />
        <div className="terminal-container">
          <div
            className="terminal"
            ref={terminalRef}
          />
        </div>
      </div>
    </div>
  );
}

const useSocketConnection = (
  onReceive: (data: any) => void,
  onOpen: (self: Socket) => any,
  onClose: (reason: Socket.DisconnectReason, description?: DisconnectDescription) => any,
) => {
  const socketRef = useRef<Socket | null>(null);
  const sessionIdRef = useRef<string | null>(null);

  // Establish websocket connection
  useEffect(() => {
    console.log("[LOG] connecting to session");

    const socket = io(`${import.meta.env.VITE_API_BASE_URL}`);
    if (!socket) {
      return;
    }

    socketRef.current = socket;

    socket.on("connect", () => onOpen(socket));
    socket.on("disconnect", (reason, description) => onClose(reason, description));
    socket.on("session_created", (data) => {
      console.log("[LOG] session created successfully")
      console.log(data)
      sessionIdRef.current = data.sid;
    });

    // Handle IO from pty
    socket.on("pty-receive", (data) => onReceive(data))

    return () => {
      console.log("[LOG] Closing socket");
      socket.close();
    }
  }, []);

  const handle_tab_close = () => {

    const socket = socketRef.current;
    if (!socket) {
      return;
    }
    console.log("[LOG] CLOSING WEBSOCKET")
    socket.close();
  }

  // Close socket when tab is closed
  useEffect(() => {
    window.addEventListener("beforeunload", handle_tab_close);
    return () => {
      window.removeEventListener("beforeunload", handle_tab_close);
    }
  }, []);
  return { socketRef, sessionIdRef };
}

export default App
