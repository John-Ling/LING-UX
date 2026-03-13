
import { Terminal } from '@xterm/xterm';
import './App.css'
import { useEffect, useRef, useState } from 'react'
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';
import type { Socket } from 'socket.io-client';
import { useSocketConnection } from './hooks/useSocketConnection';
import { useSound } from './hooks/useSound';

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
  const [appStarted, setAppStarted] = useState<boolean>(false);

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
  }

  const socketClose = () => {
    console.log("[LOG] Closing websocket. Goodbye :)");
  }

  const { socketRef } = useSocketConnection(data => socketReceiveRef.current(data), socketOpen, socketClose);
  const { playOnce: beep } = useSound("beep");
  const { playOnce: hddClick } = useSound("hdd_click");
  const { playOnce: startupClick } = useSound("startup_click");
  const { playLoop: loopIdle} = useSound("idle");

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

    startupClick();
    await sleep(100);
    loopIdle();
    await sleep(1000);
    beep();
    terminal.writeln("Starting system...");
    hddClick();
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
        "Ready!\n\r"], 150
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
    ], 300);

    await printLines(
     [
      "John Ling's Linux web terminal created to showcase his various CLI projects (mostly data structure libraries).\n\r",
      "\n\r",
      "- Feel free to explore the folders with terminal commands.\n\r",
      "- Check the README for every project with command \"about\" to learn more about each one.\n\r",
      "- Expect more projects to appear here over time, probably written in Rust.\n\r",
      "$ "
     ], 200
    );

    setSplashCompleted(true);
    terminal.focus();
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
      console.log("Resizing");
      console.log(terminal.rows, terminal.cols)
      // Resize terminal on backend
      socket.emit("resize-terminal", { "row_count": terminal.rows, "column_count": terminal.cols })
    }

  }

  // Play ambient sound effects randomly
  useEffect(() => {
   const intervalId = setInterval(() => {
    if (Math.floor(Math.random() * 3) === 0)  {
      hddClick();
    }
   }, 10000);

   return () => {
    clearInterval(intervalId);
   }
  }, []);

  // Load font
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

  // Initialise terminal
  useEffect(() => {
    if (!fontLoaded || !appStarted) {
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
    xtermRef.current = terminal;
    fitAddonRef.current = fitAddon;
    splashScreen();

    fitAddon.fit();
    const socket = socketRef.current;
    if (socket) {
      console.log("Sizing window to initial client size")
      console.log(terminal.rows, terminal.cols)
      socket.emit("resize-terminal", { "row_count": terminal.rows, "column_count": terminal.cols });
    }      

    terminal.onData(handleData);
    handleResizeRef.current = handleResize;
    window.addEventListener('resize', handleResize);

    return () => {
      if (handleResizeRef.current) {
        window.removeEventListener('resize', handleResizeRef.current);
      }
      xtermRef.current?.dispose();
    };
  }, [fontLoaded, appStarted]);

  return (
    <div className="root">
      <div className="terminal-container">
        <div className="vignette" />
        {/* <div className="terminal-overlay" /> */}
        {
          !appStarted
            ?
            <div onClick={() => setAppStarted(true)} className="click-area">
              <p>Click to Start</p>
            </div>
            :
            <div
              className="terminal"
              ref={terminalRef}
            />
        }
      </div>
    </div>
  );
}

export default App
