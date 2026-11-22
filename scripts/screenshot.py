#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""screenshot.py --out PATH [--size WxH] [--wait S]... [--click X,Y]... [--app PATH]

Capture the Niratan Qt application offscreen and write the screen to a PNG.

The application is started with QT_QPA_PLATFORM=vnc on a free local port; this
process is an RFB client that clicks the given points, waits, then reads the
whole framebuffer once and converts it with ImageMagick. The result always
lands under /tmp, so a screenshot can never enter the repository.

Defaults: --size 960x640, --app build/niratan, --out /tmp/screenshot-<epoch>.png.
Actions run in command-line order after a 1.5 s startup settle.
"""

import argparse
import os
import re
import signal
import socket
import struct
import subprocess
import sys
import time

USAGE = "screenshot.py --out PATH [--size WxH] [--wait S]... [--click X,Y]... [--app PATH]"

DEFAULT_SIZE = (960, 640)
DEFAULT_APP = "build/niratan"
SETTLE_SECONDS = 1.5
CLICK_PAUSE_SECONDS = 0.05
RFB_TIMEOUT_SECONDS = 10.0
TERMINATE_SECONDS = 3.0


class ToolError(Exception):
    """A failure reported on stderr with exit status 1."""


class OrderedAction(argparse.Action):
    """Keeps --wait and --click in the order they appeared on the command line."""

    def __call__(self, parser, namespace, values, option_string=None):
        namespace.actions.append((self.dest, values))


def parse_click(value):
    parts = value.split(",")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(f"expected X,Y, got {value!r}")
    try:
        x, y = int(parts[0]), int(parts[1])
    except ValueError:
        raise argparse.ArgumentTypeError(f"expected X,Y, got {value!r}") from None
    if x < 0 or y < 0:
        raise argparse.ArgumentTypeError(f"expected non-negative X,Y, got {value!r}")
    return (x, y)


def parse_size(value):
    match = re.fullmatch(r"(\d+)x(\d+)", value)
    if match is None:
        raise argparse.ArgumentTypeError(f"expected WxH, got {value!r}")
    width, height = int(match.group(1)), int(match.group(2))
    if width < 1 or height < 1:
        raise argparse.ArgumentTypeError(f"expected positive WxH, got {value!r}")
    return (width, height)


def parse_seconds(value):
    try:
        seconds = float(value)
    except ValueError:
        raise argparse.ArgumentTypeError(f"expected seconds, got {value!r}") from None
    if seconds < 0:
        raise argparse.ArgumentTypeError(f"expected non-negative seconds, got {value!r}")
    return seconds


def build_parser():
    parser = argparse.ArgumentParser(
        usage=USAGE,
        description="Capture the Niratan window from the Qt VNC platform plugin.",
    )
    parser.add_argument("--out", metavar="PATH", help="output PNG, must resolve under /tmp")
    parser.add_argument(
        "--size",
        metavar="WxH",
        type=parse_size,
        default=DEFAULT_SIZE,
        help="window size (default: 960x640)",
    )
    parser.add_argument(
        "--app",
        metavar="PATH",
        default=DEFAULT_APP,
        help=f"application binary (default: {DEFAULT_APP})",
    )
    parser.set_defaults(actions=[])
    parser.add_argument(
        "--wait",
        metavar="S",
        dest="wait",
        type=parse_seconds,
        action=OrderedAction,
        help="sleep S seconds before the capture (repeatable)",
    )
    parser.add_argument(
        "--click",
        metavar="X,Y",
        dest="click",
        type=parse_click,
        action=OrderedAction,
        help="click at X,Y before the capture (repeatable)",
    )
    return parser


def default_output():
    return f"/tmp/screenshot-{int(time.time())}.png"


def output_path(path):
    """Resolves path under /tmp or exits with status 2 naming the refused path."""
    resolved = os.path.realpath(path)
    if os.path.commonpath([resolved, "/tmp"]) != "/tmp":
        print(f"screenshot.py: --out must be under /tmp: {path}", file=sys.stderr)
        sys.exit(2)
    return resolved


def check_app(path):
    if not (os.path.isfile(path) and os.access(path, os.X_OK)):
        raise ToolError(f"{path} is missing or not executable; build it with: cmake --build build")


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def launch_app(app, width, height, port):
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = f"vnc:size={width}x{height}:port={port}"
    try:
        return subprocess.Popen(
            [app],
            env=environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except OSError as error:
        raise ToolError(f"cannot start {app}: {error}") from error


def stop_app(process):
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=TERMINATE_SECONDS)
    except subprocess.TimeoutExpired:
        process.send_signal(signal.SIGKILL)
        process.wait()


def recv_exact(sock, count, deadline, phase):
    data = b""
    while len(data) < count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise ToolError(f"no {phase} within {RFB_TIMEOUT_SECONDS:.0f} s")
        sock.settimeout(remaining)
        try:
            chunk = sock.recv(count - len(data))
        except socket.timeout:
            raise ToolError(f"no {phase} within {RFB_TIMEOUT_SECONDS:.0f} s") from None
        if not chunk:
            raise ToolError(f"the RFB connection closed while waiting for the {phase}")
        data += chunk
    return data


def connect_rfb(port, deadline):
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise ToolError(f"no RFB answer on port {port} within {RFB_TIMEOUT_SECONDS:.0f} s")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(min(remaining, 1.0))
        try:
            sock.connect(("127.0.0.1", port))
            return sock
        except OSError:
            sock.close()
            time.sleep(0.05)


def handshake(sock, deadline):
    version = recv_exact(sock, 12, deadline, "RFB handshake")
    sock.sendall(version)
    if version.startswith(b"RFB 003.003"):
        recv_exact(sock, 4, deadline, "RFB security result")
    else:
        count = recv_exact(sock, 1, deadline, "RFB security result")[0]
        recv_exact(sock, count, deadline, "RFB security result")
        sock.sendall(b"\x01")
        recv_exact(sock, 4, deadline, "RFB security result")
    sock.sendall(b"\x01")
    width, height = struct.unpack(">HH", recv_exact(sock, 4, deadline, "RFB server init"))
    recv_exact(sock, 16, deadline, "RFB server init")
    name_length = struct.unpack(">I", recv_exact(sock, 4, deadline, "RFB server init"))[0]
    recv_exact(sock, name_length, deadline, "RFB server init")
    return width, height


def apply_actions(sock, actions):
    for kind, value in actions:
        if kind == "click":
            x, y = value
            # A pointer move first, so the first click after startup lands on
            # an active window instead of only activating it.
            sock.sendall(struct.pack(">BBHH", 5, 0, x, y))
            time.sleep(0.03)
            sock.sendall(struct.pack(">BBHH", 5, 1, x, y))
            time.sleep(CLICK_PAUSE_SECONDS)
            sock.sendall(struct.pack(">BBHH", 5, 0, x, y))
        else:
            time.sleep(value)


def request_framebuffer(sock, width, height):
    # 32 bpp, depth 24, little endian, true colour, red 16, green 8, blue 0.
    sock.sendall(struct.pack(">B3xBBBBHHHBBB3x", 0, 32, 24, 0, 1, 255, 255, 255, 16, 8, 0))
    # Raw encoding only, so every rectangle arrives as pixels.
    sock.sendall(struct.pack(">BxHi", 2, 1, 0))
    sock.sendall(struct.pack(">BBHHHH", 3, 0, 0, 0, width, height))


def capture(sock, width, height):
    deadline = time.monotonic() + RFB_TIMEOUT_SECONDS
    framebuffer = bytearray(width * height * 4)
    covered = bytearray(width * height)
    while not all(covered):
        message = recv_exact(sock, 1, deadline, "full framebuffer")[0]
        if message == 0:
            recv_exact(sock, 1, deadline, "full framebuffer")
            (rectangles,) = struct.unpack(">H", recv_exact(sock, 2, deadline, "full framebuffer"))
            for _ in range(rectangles):
                header = recv_exact(sock, 12, deadline, "full framebuffer")
                x, y, rect_width, rect_height, encoding = struct.unpack(">HHHHi", header)
                if encoding != 0:
                    raise ToolError(f"unexpected RFB encoding {encoding}")
                pixels = recv_exact(
                    sock,
                    rect_width * rect_height * 4,
                    deadline,
                    "full framebuffer",
                )
                for row in range(rect_height):
                    offset = ((y + row) * width + x) * 4
                    framebuffer[offset : offset + rect_width * 4] = pixels[
                        row * rect_width * 4 : (row + 1) * rect_width * 4
                    ]
                    covered[(y + row) * width + x : (y + row) * width + x + rect_width] = (
                        b"\x01" * rect_width
                    )
        elif message == 2:
            continue
        elif message == 3:
            recv_exact(sock, 3, deadline, "full framebuffer")
            text_length = struct.unpack(">I", recv_exact(sock, 4, deadline, "full framebuffer"))[0]
            recv_exact(sock, text_length, deadline, "full framebuffer")
        else:
            raise ToolError(f"unexpected RFB server message {message}")
    return framebuffer


def write_png(pixels, width, height, path):
    directory = os.path.dirname(path) or "."
    os.makedirs(directory, exist_ok=True)
    temporary = f"{path}.tmp-{os.getpid()}.png"
    command = [
        "magick",
        "-size",
        f"{width}x{height}",
        "-depth",
        "8",
        "bgra:-",
        "-alpha",
        "off",
        temporary,
    ]
    try:
        subprocess.run(command, input=bytes(pixels), check=True)
        os.replace(temporary, path)
    except (OSError, subprocess.CalledProcessError) as error:
        raise ToolError(f"cannot write {path}: {error}") from error
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def run(args):
    width, height = args.size
    output = output_path(args.out or default_output())
    check_app(args.app)

    port = free_port()
    process = launch_app(args.app, width, height, port)
    try:
        deadline = time.monotonic() + RFB_TIMEOUT_SECONDS
        sock = connect_rfb(port, deadline)
        try:
            frame_width, frame_height = handshake(sock, deadline)
            if (frame_width, frame_height) != (width, height):
                raise ToolError(
                    f"the RFB server reports {frame_width}x{frame_height}, "
                    f"expected {width}x{height}"
                )
            time.sleep(SETTLE_SECONDS)
            apply_actions(sock, args.actions)
            request_framebuffer(sock, width, height)
            pixels = capture(sock, width, height)
        finally:
            sock.close()
    finally:
        stop_app(process)

    write_png(pixels, width, height, output)
    print(f"{output} {width}x{height}")
    return 0


def main():
    args = build_parser().parse_args()
    try:
        return run(args)
    except ToolError as error:
        print(f"screenshot.py: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130
    except OSError as error:
        print(f"screenshot.py: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
