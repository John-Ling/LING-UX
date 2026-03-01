import { Terminal } from '@xterm/xterm';
import './App.css'
import { io } from 'socket.io-client';
import { useEffect, useRef, useState } from 'react'
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';
import type { DisconnectDescription, Socket } from 'socket.io-client';

function App() {
  const terminalRef = useRef<HTMLDivElement | null>(null);
  const xtermRef = useRef<Terminal | null>(null);
  const fitAddonRef = useRef<FitAddon | null>(null);
  const handleResizeRef = useRef<(() => void) | null>(null);
  const [fontLoaded, setFontLoaded] = useState<boolean>(false);

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
    self.emit("create_session");

    const terminal = xtermRef.current;
    if (terminal) {
      console.log("[LOG] Setting terminal size to match initial client size");
      // Set pty size based on current client size
      self.emit("resize-terminal", { "row_count": terminal.rows, "column_count": terminal.cols });
    }

  }

  const socketClose = (reason: Socket.DisconnectReason, description?: DisconnectDescription) => {
    console.log("[LOG] Closing websocket. Goodbye :)");
  }

  const { socketRef } = useSocketConnection(socketReceive, socketOpen, socketClose);

  const handleData = (data: string) => {
    console.log("DATA", data)
    const socket = socketRef.current;
    const terminal = xtermRef.current;
    if (socket && terminal) {
      socket.emit("send-to-terminal", { "command": data })
    }
  }

  const handleResize = () => {
    const socket = socketRef.current;
    const terminal = xtermRef.current;
    const fitAddon = fitAddonRef.current;
    if (fitAddon && socket && terminal) {
      fitAddon.fit();
      // Resize pty on backend
      socket.emit("resize-terminal", { "row_count": terminal.rows, "column_count": terminal.cols })
    }

  }

  useEffect(() => {
    const loadFont = async () => {
      await document.fonts.load("1em PixelCode");
      setFontLoaded(true);
    }
    loadFont();
    return () => {
      document.fonts.clear();
      setFontLoaded(false);
    }
  }, [])

  useEffect(() => {
    if (!fontLoaded) {
      return;
    }

    const terminal = new Terminal({
      fontFamily: "PixelCode",
      cursorStyle: "block",
      theme: { 
        background: "#1c0902",
        foreground: "#e5a700",
        cursor: "#e5a700",
        cursorAccent: "#1c0902",
        selectionForeground: "#1c0902",
        selectionInactiveBackground: "#e5a700",
        selectionBackground: "#e5a700"
       }
    });

    const fitAddon = new FitAddon();
    terminal.loadAddon(fitAddon);

    if (!terminalRef.current) return;

    terminal.open(terminalRef.current);
    fitAddon.fit();

    xtermRef.current = terminal;
    fitAddonRef.current = fitAddon;
    // terminal.write("Hello World");

    terminal.onData(handleData);

    handleResizeRef.current = handleResize;
    window.addEventListener('resize', handleResize);

    return () => {
      if (handleResizeRef.current) {
        window.removeEventListener('resize', handleResizeRef.current);
      }
      xtermRef.current?.dispose();
    };
  }, [fontLoaded]);

  return (
    <div className="root">
        <div className="terminal-container">
          <div className="vignette" />
          {/* <div className="terminal-overlay"/> */}
          <div
            className="terminal"
            ref={terminalRef}
          />
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
