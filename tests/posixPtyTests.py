#!/usr/bin/env python3
"""POSIX pseudo-terminal integration tests for terminalTool restoration."""

import errno
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time

ENTER_ALT = b"\x1b[?1049h"
LEAVE_ALT = b"\x1b[?1049l"
INJECTED_TITLE = b"\x1b]0;injected"


def set_size(fd: int, rows: int = 24, columns: int = 80) -> None:
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))


def drain(fd: int, timeout: float = 0.05) -> bytes:
    chunks = []
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        readable, _, _ = select.select([fd], [], [], max(0.0, end - time.monotonic()))
        if not readable:
            break
        try:
            data = os.read(fd, 65536)
        except OSError as error:
            if error.errno == errno.EIO:
                break
            raise
        if not data:
            break
        chunks.append(data)
    return b"".join(chunks)


class Child:
    def __init__(self, pid: int, master: int):
        self.pid = pid
        self.master = master
        self.returncode = None

    def send_signal(self, signal_number: int) -> None:
        os.kill(self.pid, signal_number)

    def poll(self):
        if self.returncode is not None:
            return self.returncode
        pid, status = os.waitpid(self.pid, os.WNOHANG)
        if pid == 0:
            return None
        self.returncode = os.waitstatus_to_exitcode(status)
        return self.returncode

    def wait(self, timeout: float = 3.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            result = self.poll()
            if result is not None:
                return result
            time.sleep(0.01)
        raise TimeoutError("child process did not exit")

    def kill(self):
        if self.poll() is None:
            os.kill(self.pid, signal.SIGKILL)
            self.wait()


def spawn(executable: str, *arguments: str):
    pid, master = pty.fork()
    if pid == 0:
        os.execv(executable, [executable, *arguments])
        raise AssertionError("execv returned")

    set_size(master)
    child = Child(pid, master)
    time.sleep(0.12)
    output = drain(master, 0.15)
    return child, output


def finish_with_escape(child: Child, output: bytes) -> bytes:
    os.write(child.master, b"\x1b")
    deadline = time.monotonic() + 3.0
    while child.poll() is None and time.monotonic() < deadline:
        output += drain(child.master, 0.05)
        time.sleep(0.02)
    output += drain(child.master, 0.1)
    if child.poll() is None:
        child.kill()
        raise AssertionError("probe did not exit after Escape")
    os.close(child.master)
    return output


def test_normal(executable: str) -> None:
    child, output = spawn(executable)
    output = finish_with_escape(child, output)
    assert child.returncode == 0, child.returncode
    assert ENTER_ALT in output
    assert LEAVE_ALT in output
    assert INJECTED_TITLE not in output, "POSIX title control injection was not sanitized"


def test_previous_handler(executable: str) -> None:
    child, output = spawn(executable, "--previous-term-handler")
    child.send_signal(signal.SIGTERM)
    child.wait()
    output += drain(child.master, 0.15)
    os.close(child.master)
    assert child.returncode == 77, child.returncode
    assert LEAVE_ALT in output



def test_returning_previous_handler(executable: str) -> None:
    child, output = spawn(executable, "--returning-term-handler")
    child.send_signal(signal.SIGTERM)
    time.sleep(0.18)
    output += drain(child.master, 0.2)
    assert child.poll() is None, "a returning previous handler unexpectedly terminated the probe"
    assert output.count(LEAVE_ALT) >= 1, "terminal was not restored before the previous handler"
    assert output.count(ENTER_ALT) >= 2, "terminal state was not reapplied after the previous handler returned"
    output = finish_with_escape(child, output)
    assert child.returncode == 0, child.returncode


def test_suspend_resume(executable: str) -> None:
    child, output = spawn(executable)
    child.send_signal(signal.SIGTSTP)
    deadline = time.monotonic() + 3.0
    stopped = False
    while time.monotonic() < deadline:
        pid, status = os.waitpid(child.pid, os.WUNTRACED | os.WNOHANG)
        if pid == child.pid and os.WIFSTOPPED(status):
            stopped = True
            break
        output += drain(child.master, 0.03)
        time.sleep(0.02)
    assert stopped, "probe did not suspend"
    output += drain(child.master, 0.1)
    assert LEAVE_ALT in output, "terminal was not restored before SIGTSTP"

    child.send_signal(signal.SIGCONT)
    time.sleep(0.15)
    output += drain(child.master, 0.15)
    output = finish_with_escape(child, output)
    assert child.returncode == 0, child.returncode
    assert output.count(ENTER_ALT) >= 2, "alternate screen was not reapplied after SIGCONT"
    assert output.count(LEAVE_ALT) >= 2, "terminal was not restored after resume and exit"


def main() -> int:
    executable = sys.argv[1]
    test_normal(executable)
    test_previous_handler(executable)
    test_returning_previous_handler(executable)
    test_suspend_resume(executable)
    print("All POSIX pseudo-terminal tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
