import { Terminal } from '@xterm/xterm';
import './App.css'
import { useCallback, useEffect, useRef } from 'react'
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';

function App() {
  const terminalRef = useRef<HTMLDivElement | null>(null);
  const xtermRef = useRef<Terminal | null>(null);
  const fitAddonRef = useRef<FitAddon | null>(null);

  const handle_key_down = (arg1: { key: string, domEvent: KeyboardEvent }) => {
    if (!xtermRef.current) {
      return;
    }

    const terminal = xtermRef.current;
    const event = arg1.domEvent;

    if (event.key === "Enter") {
      terminal.write("\n\r");
      init();
    } else if (event.ctrlKey && event.key.toLowerCase() === 'c') {
      event.preventDefault();
      terminal.write("\x03");
    } else {
      terminal.write(event.key)
    }
  }

  const init = () => {
    if (!xtermRef.current) {
      return;
    }

    const terminal = xtermRef.current;
    terminal.write("jimbob@workstation ~ $ ");
  }
  
  const handle_socket_open = () => {
    console.log("[LOG] Connected to websocket")
  } 

  const handle_socket_receive = (ev: MessageEvent) => {
    console.log("[LOG] Received data");
    console.log(ev.data);
    const terminal = xtermRef.current;
    if (terminal) {
      terminal.write(ev.data + '\n')
    }
  }

  // Create websocket connection
  useEffect(() => {
    console.log("[DEBUG] connecting to websocket");
    const socket = new WebSocket("ws://localhost:8000/session/12345");
    if (!socket) {
      return;
    }

    socket.onmessage = ev => {
      handle_socket_receive(ev)
    }
    socket.onopen = ev => {
      handle_socket_open();
    }

    return () => {
    }
  }, []);

  // Init xterm.js
  useEffect(() => {
    const terminal = new Terminal({
      cursorBlink: true,
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

    init();

    terminal.onKey(handle_key_down)

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

export default App
