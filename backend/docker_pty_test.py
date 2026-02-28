import docker
import pty
import os
import select
import sys
import threading

client = docker.from_env()


def create_session(session_id: str):
    """Spin up a lightweight Alpine container for a terminal session."""
    container = client.containers.run(
        "alpine:latest",
        command="/bin/sh",
        name=f"session_{session_id}",
        detach=True,
        tty=True,
        stdin_open=True,
        network_mode="none",
        mem_limit="128m",
        nano_cpus=500_000_000,
        cap_drop=["ALL"],
        security_opt=["no-new-privileges:true"],
        remove=False,
        tmpfs={"/home": "size=64m,mode=1777"},

    )
    print(f"[+] Container session_{session_id} created")
    return container


def attach_pty(container):
    """
    Attach to the container's PTY and relay I/O to the current terminal.
    Uses the Docker SDK's attach_socket for a raw stream.
    """
    # Get a raw socket to the container's stdin/stdout
    sock = container.attach_socket(
        params={"stdin": 1, "stdout": 1, "stderr": 1, "stream": 1}
    )
    sock = sock._sock  # unwrap to raw socket

    print("[*] Attached — type 'exit' to quit\n")

    # Set terminal to raw mode so keypresses go straight through
    import tty
    import termios

    old_settings = termios.tcgetattr(sys.stdin)

    try:
        tty.setraw(sys.stdin.fileno())

        def read_from_container():
            while True:
                try:
                    data = sock.recv(1024)
                    if not data:
                        break
                    os.write(sys.stdout.fileno(), data)
                except OSError:
                    break

        # Read container output in a background thread
        t = threading.Thread(target=read_from_container, daemon=True)
        t.start()

        # Read local keypresses and send to container
        while t.is_alive():
            r, _, _ = select.select([sys.stdin], [], [], 0.1)
            if r:
                data = os.read(sys.stdin.fileno(), 1024)
                sock.sendall(data)

    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
        sock.close()


def destroy_session(container):
    """Stop and remove the container, freeing all its storage."""
    try:
        container.stop(timeout=5)
    except Exception:
        container.kill()
    container.remove()
    print(f"[-] Session {container.name} destroyed")


# ── Demo ──────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    container = create_session("alice")
    try:
        attach_pty(container)
    finally:
        destroy_session(container)
