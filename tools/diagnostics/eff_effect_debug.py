"""Local opt-in EFF effect debugger client. No dependency on Blender or index files."""
import argparse
import ctypes
from ctypes import wintypes
import time


def exchange(pid, command, timeout=5.0):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.SetNamedPipeHandleState.argtypes = (wintypes.HANDLE, wintypes.LPDWORD,
                                               wintypes.LPVOID, wintypes.LPVOID)
    kernel.ReadFile.argtypes = kernel.WriteFile.argtypes = (
        wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD, wintypes.LPDWORD, wintypes.LPVOID)
    kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
    kernel.WaitNamedPipeW.argtypes = (wintypes.LPCWSTR, wintypes.DWORD)
    name = rf"\\.\pipe\EFF-Effects-{pid}"
    deadline = time.monotonic() + timeout
    while True:
        handle = kernel.CreateFileW(name, 0xC0000000, 0, None, 3, 0, None)
        if handle != ctypes.c_void_p(-1).value:
            break
        code = ctypes.get_last_error()
        if code not in (2, 231) or time.monotonic() >= deadline:
            raise OSError(code, "EFF effects-debug pipe is unavailable", name)
        if code == 231:
            kernel.WaitNamedPipeW(name, 100)
        else:
            time.sleep(.005)
    try:
        mode = wintypes.DWORD(2)
        if not kernel.SetNamedPipeHandleState(handle, ctypes.byref(mode), None, None):
            raise ctypes.WinError(ctypes.get_last_error())
        packet = command.encode("utf-8")
        if b"\0" in packet or len(packet) > 65536:
            raise ValueError("Invalid effect pipe command")
        size = wintypes.DWORD()
        if not kernel.WriteFile(handle, ctypes.c_char_p(packet), len(packet), ctypes.byref(size), None):
            raise ctypes.WinError(ctypes.get_last_error())
        chunks = []
        while True:
            buffer = ctypes.create_string_buffer(65536)
            done = kernel.ReadFile(handle, buffer, len(buffer), ctypes.byref(size), None)
            chunks.append(buffer.raw[:size.value])
            if done:
                break
            if ctypes.get_last_error() != 234:
                raise ctypes.WinError(ctypes.get_last_error())
        # Acknowledgement lets the server close without discarding an unread
        # buffered response. It also avoids blocking FlushFileBuffers.
        kernel.WriteFile(handle, ctypes.c_char_p(b"\x06"), 1, ctypes.byref(size), None)
        return b"".join(chunks).decode("utf-8", errors="strict")
    finally:
        kernel.CloseHandle(handle)


def execute(pid, command, timeout=30.0):
    reply = exchange(pid, command)
    if reply.startswith("QUEUED\t"):
        ticket = int(reply.split("\t")[1])
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            reply = exchange(pid, f"RESULT\t{ticket}")
            if reply != "PENDING\n":
                break
            time.sleep(0.025)
        else:
            raise TimeoutError(f"Unity command {ticket} is still pending; timeout does not cancel it")
    if reply.startswith("ERR\t"):
        raise RuntimeError(reply[4:].strip())
    return reply


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("command", choices=("status", "owners", "nodes", "prepare", "play", "stop", "clear"))
    parser.add_argument("args", nargs="*")
    args = parser.parse_args()
    print(execute(args.pid, "\t".join([args.command.upper(), *args.args]), args.timeout), end="")
