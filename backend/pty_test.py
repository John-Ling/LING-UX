"""
pty_bash.py — Pseudo-terminal example using Python's pty module.

Creates a bash process attached to a PTY, sends a command,
and reads back the output.
"""

import os
import pty
import time
import select
import termios
import tty


def read_until_prompt(fd: int, prompt: str = "$ ", timeout: float = 2.0) -> str:
    """Read from fd until we see a shell prompt or hit the timeout."""
    output = ""
    deadline = time.time() + timeout

    process_ended = False

    while not process_ended:
        remaining = deadline - time.time()
        ready, _, _ = select.select([fd], [], [], remaining)
        if not ready:
            print("Not ready")
            break
        try:
            chunk = os.read(fd, 1024).decode("utf-8", errors="replace")
        except OSError:
            print("OS ERROR")
            break
        output += chunk
        if chunk.endswith(prompt) or output.rstrip().endswith(prompt.strip()):
            process_ended = True
            continue

    return output


def send_command(fd: int, command: str) -> None:
    """Write a command (with newline) to the PTY master fd."""
    os.write(fd, (command + "\n").encode())

def main():
    # Fork a child process connected to a PTY
    pid, master_fd = pty.fork()
    print(pid)


    print(pid)

    if pid == 0:
        # ---- Child process: exec bash ----
        env = os.environ.copy()
        env["TERM"] = "dumb"
        env["PS1"] = "$ "
        env["ZDOTDIR"] = "/nonexistent"   # stops zsh from loading .zshrc
        env.pop("ZSH", None)
        env.pop("ZSH_THEME", None)
        os.execvpe("sh", ["sh"], env)
        # execvp never returns on success; this line won't run
        raise RuntimeError("execvp failed")

    # ---- Parent process ----
    try:
        # Give bash a moment to start, then wait for the initial prompt
        time.sleep(0.2)
        startup_output = read_until_prompt(master_fd, prompt="$ ", timeout=2.0)
        print("[startup output]\n", repr(startup_output))

        # --- Send a command and read the response ---
        command = "echo 'Hello from PTY!'"
        print(f"\n[sending command]: {command!r}")
        send_command(master_fd, command)

        response = read_until_prompt(master_fd, prompt="$ ", timeout=2.0)
        print(f"[raw response]:\n{response}")

        # Strip the echoed command and trailing prompt to get just stdout
        lines = response.splitlines()
        # First line is the echoed command; last line is the next prompt
        output_lines = lines[1:-1] if len(lines) > 2 else lines
        print(f"[clean output]: {chr(10).join(output_lines)}")

        # --- Send another command ---
        command2 = "pwd"
        print(f"\n[sending command]: {command2!r}")
        send_command(master_fd, command2)

        response2 = read_until_prompt(master_fd, prompt="$ ", timeout=2.0)
        lines2 = response2.splitlines()
        output_lines2 = lines2[1:-1] if len(lines2) > 2 else lines2
        print(f"[clean output]: {chr(10).join(output_lines2)}")

        # --- Exit bash cleanly ---
        send_command(master_fd, "exit")
        time.sleep(0.2)

    finally:
        os.close(master_fd)
        os.waitpid(pid, 0)
        print("\n[bash process exited cleanly]")


if __name__ == "__main__":
    main()