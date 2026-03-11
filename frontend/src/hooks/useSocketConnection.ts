import { Socket } from "socket.io-client";
import type { DisconnectDescription } from "socket.io-client";
import { io } from "socket.io-client";
import { useRef, useEffect } from "react";

export const useSocketConnection = (
	onReceive: (data: any) => void,
	onOpen: (self: Socket) => any,
	onClose: (reason: Socket.DisconnectReason, description?: DisconnectDescription) => any,
) => {
	const socketRef = useRef<Socket | null>(null);
	const sessionIdRef = useRef<string | null>(null);

	// Establish websocket connection
	useEffect(() => {
		console.log(`[LOG] connecting to session at ${import.meta.env.VITE_API_BASE_URL}`);

		// Socket.IO appends the path to the URL, so we need the base domain
		// VITE_API_BASE_URL is "https://terminal.johnling.me/api" but Socket.IO needs just the domain
		const socket = io(import.meta.env.VITE_API_BASE_URL.replace(/\/api$/, ""), {
			path: "/api/socket.io"
		});
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

		// Handle IO from terminal
		socket.on("terminal-receive", (data) => onReceive(data))

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
