
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

  // Store the socket's receive method as a ref to keep up to date with splashCompleted state
  const socketReceiveRef = useRef((data: any) => {
    const terminal = xtermRef.current;
    if (!terminal) return;

    if (data["output"] && splashCompleted) {
      terminal.write(data["output"]);
    }
  });
  const handleResizeRef = useRef<(() => void) | null>(null);
  const [splashCompleted, setSplashCompleted] = useState<boolean>(false);
  const [fontLoaded, setFontLoaded] = useState<boolean>(true);

  // Update the ref when dependencies change
  useEffect(() => {
    socketReceiveRef.current = (data: any) => {
      const terminal = xtermRef.current;
      if (!terminal) return;

      if (data["output"] && splashCompleted) {
        terminal.write(data["output"]);
      }
    };
  }, [splashCompleted]);

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

  const { socketRef } = useSocketConnection(data => socketReceiveRef.current(data), socketOpen, socketClose);

  const splashScreen = async () => {
    const terminal = xtermRef.current;
    if (!terminal) {
      return;
    }

    async function printLines(lines: string[], delayMs = 200) {
      for (const line of lines) {
        terminal!.write(line);
        await sleep(delayMs);
      }
    }

    const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));

    terminal.writeln("Starting system...");
    await sleep(500);
    await printLines([
      "Linux version 6.8.0-47-generic (buildd@lcy02-amd64-080) (x86_64-linux-gnu-gcc-13 (Ubuntu 13.2.0-23ubuntu4) 13.2.0, GNU ld (GNU Binutils for Ubuntu) 2.42) #47-Ubuntu SMP PREEMPT_DYNAMIC Sun August  7 07:51:54 UTC 1982\n\r",
      "Command line: BOOT_IMAGE=/boot/vmlinuz-6.8.0-47-generic root=UUID=a1b2c3d4-e5f6-7890-abcd-ef1234567890 ro quiet splash vt.handoff=7\n\r",
      "BIOS-provided physical RAM map:\n\r"], 150);

    await printLines(
      ["BIOS-e820: [mem 0x0000000000000000-0x0000000000000fff] reserved\n\r",
        "BIOS-e820: [mem 0x0000000000001000-0x000000000009ffff] usable\n\r",
        "BIOS-e820: [mem 0x00000000000a0000-0x00000000000fffff] reserved\n\r",
        "BIOS-e820: [mem 0x0000000000100000-0x000000007f86bfff] usable\n\r",
        "BIOS-e820: [mem 0x000000007f86c000-0x000000007f87bfff] ACPI NVS\n\r",
        "BIOS-e820: [mem 0x000000007f87c000-0x000000007fce5fff] usable\n\r",
        "BIOS-e820: [mem 0x000000007fce6000-0x000000007feedfff] reserved\n\r",
        "BIOS-e820: [mem 0x000000007feee000-0x000000007fef8fff] usable\n\r",
        "BIOS-e820: [mem 0x000000007fef9000-0x000000007ff0afff] ACPI data\n\r",
        "BIOS-e820: [mem 0x000000007ff0b000-0x000000007ff0bfff] usable\n\r",
        "BIOS-e820: [mem 0x000000007ff0c000-0x00000000829fffff] reserved\n\r",
        "BIOS-e820: [mem 0x00000000ffd10000-0x00000000ffd4ffff] reserved\n\r",
        "BIOS-e820: [mem 0x0000000100000000-0x000000027d5fffff] usable\n\r"],
      100
    );

    await printLines(
      ["NX (Execute Disable) protection: active\n\r",
        "APIC: Static calls initialized\n\r",
        "SMBIOS 3.0.0 ",
        "present\n\r",
        "Secure boot disabled\n\r",
        "RAMDISK: [mem 0x745da000-0x764a9fff]\n\r",
        "ACPI: Early table checksum verification disabled\n\r",],
      200
    )

    await printLines(
      ["ACPI: RSDP 0x000000007FF0A014 000024 (v02 COREv4)\n\r",
        "ACPI: XSDT 0x000000007FF090E8 000064 (v01 COREv4 COREBOOT 00000000      01000013)\n\r",
        "ACPI: FACP 0x000000007FF08000 000114 (v06 COREv4 COREBOOT 00000000 CORE 20230628)\n\r",
        "ACPI: FACS 0x000000007FF24240 000040\n\r",
        "ACPI: TCPA 0x000000007FEFF000 000032 (v02 COREv4 COREBOOT 00000000 CORE 20230628)\n\r",
        "ACPI: HPET 0x000000007FEFD000 000038 (v01 COREv4 COREBOOT 00000000 CORE 20230628)\n\r",
        "ACPI: TCPA 0x000000007FEFC000 000032 (v02 INTEL  EDK2     00000002      01000013)\n\r",
        "Ready!\n\r"], 100
    );

    await sleep(200);
    terminal.clear();
    await printLines([
      "██╗     ██╗███╗  ██╗ ██████╗       ██╗   ██╗██╗  ██╗\n\r",
      "██║     ██║████╗ ██║██╔════╝       ██║   ██║╚██╗██╔╝\n\r",
      "██║     ██║██╔██╗██║██║  ██╗ █████╗██║   ██║ ╚███╔╝ \n\r",
      "██║     ██║██║╚████║██║  ╚██╗╚════╝██║   ██║ ██╔██╗ \n\r",
      "███████╗██║██║ ╚███║╚██████╔╝      ╚██████╔╝██╔╝╚██╗\n\r",
      "╚══════╝╚═╝╚═╝  ╚══╝ ╚═════╝        ╚═════╝ ╚═╝  ╚═╝\n\r",
      "$ "
    ], 300);

    setSplashCompleted(true);
  }

  const handleData = (data: string) => {
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

    splashScreen();

    console.log("[LOG] Splash screen completed. Unblocking");

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
