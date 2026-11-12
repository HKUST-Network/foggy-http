#!/usr/bin/env python3
"""Check CP4 routes, errors, and OPTIONS on a local mock server."""
import argparse
import http.client
import io
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(binary, *arguments):
    result = subprocess.run([binary, *map(str, arguments)], capture_output=True, timeout=15)
    check(result.returncode == 0,
          f"{binary.name} {' '.join(map(str, arguments))}: {result.stderr.decode(errors='replace').strip()}")
    return result.stdout


def request(build, port, method, path):
    wire = run(build / "foggy-http-probe", port, method, path)
    head, separator, body = wire.partition(b"\r\n\r\n")
    status_line, _, headers = head.partition(b"\r\n")
    status = status_line.split()
    check(separator and len(status) >= 2 and status[0] == b"HTTP/1.1" and status[1].isdigit(),
          f"{method} {path} did not return a valid HTTP/1.1 response")
    return int(status[1]), http.client.parse_headers(io.BytesIO(headers + b"\r\n\r\n")), body


def run_checks(build, port):
    client = build / "foggy-client"
    health = json.loads(run(client, "--port", port, "health"))
    check(health == {"status": "ok", "model": "course-model"}, "unexpected health response")
    models = json.loads(run(client, "--port", port, "models"))
    model = {"id": "course-model", "object": "model", "owned_by": "mock"}
    check(models == {"object": "list", "data": [model]}, "unexpected model list")
    answer = run(client, "--port", port, "chat", "--prompt", "Say hello.")
    check(b"mock messages=1: Say hello." in answer, "unexpected chat response")
    print("PASS registered routes")

    for method, path, expected, allow in (
        ("GET", "/missing", 404, None),
        ("POST", "/health", 405, "GET, OPTIONS"),
        ("OPTIONS", "/missing", 404, None),
    ):
        status, headers, body = request(build, port, method, path)
        check(status == expected, f"{method} {path} should return {expected}, got {status}")
        check(headers.get("Allow") == allow, f"{method} {path} returned an unexpected Allow header")
        error = json.loads(body)
        check(headers.get("Content-Type", "").startswith("application/json") and
              isinstance(error, dict) and "error" in error, f"{method} {path} should return a JSON error")
        print(f"PASS {method} {path} -> {status}")

    for path, allow in (("/health?check=1", "GET, OPTIONS"),
                        ("/v1/chat/completions", "POST, OPTIONS")):
        status, headers, body = request(build, port, "OPTIONS", path)
        check(status == 200 and not body, f"OPTIONS {path} should return 200 with an empty body")
        check(headers.get("Allow") == allow, f"OPTIONS {path} should return Allow: {allow}")
        print(f"PASS OPTIONS {path} -> {allow}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path(__file__).resolve().parents[2] / "build")
    build = parser.parse_args().build_dir.resolve()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="foggy-llama-smoke-") as directory:
        log = Path(directory) / "server.log"
        with log.open("wb") as output, subprocess.Popen(
            [build / "foggy-server", "--mock", "--port", str(port), "--mock-delay-ms", "0"],
            stdout=output, stderr=subprocess.STDOUT,
        ) as server:
            try:
                deadline = time.monotonic() + 5
                while "READY " not in log.read_text(errors="replace"):
                    check(server.poll() is None and time.monotonic() < deadline,
                          f"server failed to start\n{log.read_text(errors='replace')}")
                    time.sleep(0.05)
                time.sleep(0.1)  # READY is printed just before the listener opens.
                run_checks(build, port)
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        raise SystemExit(f"FAIL: {error}") from None
