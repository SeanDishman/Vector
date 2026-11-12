import argparse
import json
from pathlib import Path
import socket
import subprocess
import threading
import time


class Fixture:
    def __init__(self):
        self.sockets = []
        self.stop = threading.Event()
        self.hits = []
        self.silent = False
        for octet in range(2, 48):
            self.address = f"127.0.7.{octet}"
            try:
                for port in (81, 6379, 9100):
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    self.sockets.append(sock)
                    sock.bind((self.address, port))
                    sock.listen(64)
                    sock.settimeout(.1)
                break
            except OSError:
                for sock in self.sockets:
                    sock.close()
                self.sockets = []
        else:
            raise RuntimeError("No unused loopback fixture address")
        self.threads = []
        for sock in self.sockets:
            thread = threading.Thread(target=self.serve, args=(sock,), daemon=True)
            thread.start()
            self.threads.append(thread)

    def serve(self, sock):
        port = sock.getsockname()[1]
        while not self.stop.is_set():
            try:
                connection, _ = sock.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            with connection:
                connection.settimeout(2)
                try:
                    if port == 81 and not self.silent:
                        connection.sendall(b"SSH-2.0-OpenSSH_9.8\r\n")
                    data = connection.recv(2048)
                    self.hits.append((port, data))
                    if port == 6379 and data == b"PING\r\n":
                        connection.sendall(b"+PONG\r\n")
                except (TimeoutError, ConnectionError):
                    pass

    def close(self):
        self.stop.set()
        for sock in self.sockets:
            sock.close()
        for thread in self.threads:
            thread.join(3)


def tcp_rows(report):
    return {s["port"]: s for s in report["services"] if s["transport"] == "tcp"}


def validate_deep(report):
    coverage = report["tcp_coverage"]
    ports = set(coverage["selected_ports"])
    assert coverage["profile"] == "deep_ip", coverage
    assert ports.issuperset(range(1, 1025)) and ports.issuperset({6379, 9100, 27017})
    assert 1024 < len(ports) < 1200 and len(ports) == coverage["planned"]
    assert coverage["completed"] == coverage["planned"], coverage
    rows = tcp_rows(report)
    assert set(rows) == ports
    assert rows[81]["identified_service"] == "SSH", rows[81]
    assert rows[6379]["identified_service"] == "Redis" and rows[6379]["active_probe"], rows[6379]
    assert rows[9100]["state"] == "open" and not rows[9100]["active_probe"] and not rows[9100]["banner"]
    active, peak = set(), 0
    for event in report["activity"]:
        if event["stage"] != "services" or not event["item"]:
            continue
        if event["state"].lower() == "running":
            active.add(event["item"])
            peak = max(peak, len(active))
        else:
            active.discard(event["item"])
    assert not active and 1 < peak <= coverage["max_concurrent"] == 32, (peak, active)
    return {"planned": len(ports), "completed": coverage["completed"], "peak_active": peak}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path)
    parser.add_argument("helper", type=Path)
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    fixture = Fixture()
    results = {}
    try:
        def run(name, target, *extra):
            started = time.monotonic()
            process = subprocess.run([str(args.exe.resolve()), "--target", target, "--no-browser", "--timeout", "100", "--http-timeout", "100", *extra],
                                     capture_output=True, text=True, timeout=70)
            assert process.returncode == 0, (name, process.stderr)
            report = json.loads(process.stdout)
            (args.out / f"{name}.json").write_text(process.stdout, encoding="utf-8")
            results[name] = {"elapsed_seconds": round(time.monotonic() - started, 3)}
            return report

        deep = run("bare-ip", fixture.address)
        results["bare-ip"].update(validate_deep(deep))
        assert all(not data for port, data in fixture.hits if port == 9100), fixture.hits
        assert any(port == 6379 and data == b"PING\r\n" for port, data in fixture.hits)
        assert any(f["name"] == "OpenSSH" and f["evidence"] for f in deep["findings"])
        explicit = run("explicit-port", f"{fixture.address}:81")
        assert set(tcp_rows(explicit)) == {81} and explicit["tcp_coverage"]["profile"] == "endpoint"
        url = run("ip-url", f"http://{fixture.address}:81/")
        assert set(tcp_rows(url)) == {81} and url["tcp_coverage"]["profile"] == "endpoint"
        custom = run("custom", fixture.address, "--ports", "81,6379")
        assert set(tcp_rows(custom)) == {81, 6379} and custom["tcp_coverage"]["profile"] == "custom"
        web = run("web-only", fixture.address, "--web-only")
        assert set(tcp_rows(web)) == {80} and web["tcp_coverage"]["profile"] == "endpoint"
        fixture.silent = True
        started = time.monotonic()
        process = subprocess.run([str(args.helper.resolve()), "--cancel", fixture.address], capture_output=True, text=True, timeout=20)
        assert process.returncode == 130, process.stderr
        report = json.loads(process.stdout)
        assert report["status"] == "cancelled" and report["tcp_coverage"]["profile"] == "deep_ip"
        assert report["tcp_coverage"]["completed"] < report["tcp_coverage"]["planned"]
        assert len(tcp_rows(report)) < report["tcp_coverage"]["planned"]
        events = [e for e in report["activity"] if e["detail"] == "Connected; waiting for a service banner"]
        assert events and report["duration_ms"] - events[-1]["elapsed_ms"] < 1000, report["duration_ms"]
        (args.out / "cancelled.json").write_text(process.stdout, encoding="utf-8")
        results["cancelled"] = {"elapsed_seconds": round(time.monotonic() - started, 3), "completed": report["tcp_coverage"]["completed"]}
        (args.out / "verification.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
        print("Six loopback scenarios passed: deeper IP, explicit port, IP URL, custom list, web-only and active cancellation.")
    finally:
        fixture.close()


if __name__ == "__main__":
    main()
