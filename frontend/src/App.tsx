import { Terminal } from '@xterm/xterm';
import './App.css'
import { useEffect, useRef } from 'react'
import { useSocketConnection } from './hooks/useSocketConnection';
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';

function App() {
  const terminalRef = useRef<HTMLDivElement | null>(null);
  const xtermRef = useRef<Terminal | null>(null);
  const fitAddonRef = useRef<FitAddon | null>(null);

  const socketReceive = (ev: MessageEvent) => {
    console.log("Received data")
    const terminal = xtermRef.current;
    if (!terminal) {
      return;
    }

    console.log(ev.data)
    terminal.write(ev.data);
  }

  const socketOpen = (self: WebSocket, ev: Event) => {
    console.log("[LOG] Opening websocket connection");

    // Check cookies for existing session and connect to that instead if it exists
  }

  const socketClose = (self: WebSocket, ev: CloseEvent) => {
    console.log("[LOG] Closing websocket. Goodbye :)");
  }

  const { socketRef } = useSocketConnection(socketReceive, socketOpen, socketClose);

  const handleKeyDown = (arg1: { key: string, domEvent: KeyboardEvent }) => {
    if (!xtermRef.current) {
      return;
    }

    const terminal = xtermRef.current;
    const event = arg1.domEvent;

    if (event.key === "Enter") {
      terminal.write("\n\r");
      initPrompt();
    } else if (event.ctrlKey && event.key.toLowerCase() === 'c') {
      event.preventDefault();
      terminal.write("\x03");
    } else {
      terminal.write(event.key)
    }
  }

  const initPrompt = () => {
    if (!xtermRef.current) {
      return;
    }

    const terminal = xtermRef.current;
    terminal.write("john@workstation ~ $ ");
  }

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

    initPrompt();
    terminal.onKey(handleKeyDown);

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

export default App
