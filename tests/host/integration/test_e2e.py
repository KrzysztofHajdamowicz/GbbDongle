import json
import os
import pty
import select
import shutil
import socket
import subprocess
import threading
import time
import tty
import uuid
from pathlib import Path

import paho.mqtt.client as mqtt
import pytest


REQUEST = bytes.fromhex("010300000002C40B")
RESPONSE = bytes.fromhex("010304000A01025A60")


def rtu_frame(payload):
    crc = 0xFFFF
    for byte in payload:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return payload + bytes((crc & 0xFF, crc >> 8))


WRITE_REQUEST = rtu_frame(bytes.fromhex("010600010003"))
EXCEPTION_RESPONSE = rtu_frame(bytes.fromhex("018302"))


def wait_until(predicate, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.02)
    raise AssertionError("condition was not met before timeout")


def host_stats(host):
    return {
        key: int(value)
        for key, value in (
            field.split("=", 1) for field in host.control("STATS").split()
        )
    }


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def mqtt_client(client_id):
    if hasattr(mqtt, "CallbackAPIVersion"):
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    return mqtt.Client(client_id=client_id)


class Broker:
    def __init__(self, tmp_path):
        self.port = free_port()
        self.config = tmp_path / "mosquitto.conf"
        self.config.write_text(
            f"listener {self.port} 127.0.0.1\nallow_anonymous true\npersistence false\n",
            encoding="utf-8",
        )
        self.process = None

    def start(self):
        self.process = subprocess.Popen(
            ["mosquitto", "-c", str(self.config)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
        )

        def accepts_connections():
            if self.process.poll() is not None:
                error = self.process.stderr.read()
                raise AssertionError(f"mosquitto exited: {error}")
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.1):
                    return True
            except OSError:
                return False

        wait_until(accepts_connections)

    def stop(self):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            self.process.wait(timeout=5)


@pytest.fixture
def broker(tmp_path):
    if shutil.which("mosquitto") is None:
        pytest.skip("mosquitto executable is not installed")
    instance = Broker(tmp_path)
    instance.start()
    yield instance
    instance.stop()


@pytest.fixture
def host_binary():
    configured = os.environ.get("GBB_HOST_BINARY")
    candidate = Path(configured) if configured else Path("build/host/gbb_dongle_host")
    if not candidate.is_file():
        pytest.skip(f"host runner not built: {candidate}")
    return candidate.resolve()


class HostProcess:
    def __init__(self, binary, broker, tmp_path, response_timeout=250, persist=False):
        self.master, slave = pty.openpty()
        tty.setraw(self.master)
        self.slave_path = os.ttyname(slave)
        # sockaddr_un paths are limited to roughly 104 bytes on macOS; pytest's
        # deeply nested temporary path can exceed that even though Linux CI is fine.
        self.control_path = Path("/tmp") / f"gbb-{uuid.uuid4().hex}.sock"
        self.state_file = tmp_path / "emergency.json"
        self.stderr_path = tmp_path / f"host-{uuid.uuid4().hex}.stderr"
        self.stderr_file = self.stderr_path.open("w+", encoding="utf-8")
        self.process = subprocess.Popen(
            [
                str(binary),
                "--broker",
                "127.0.0.1",
                "--port",
                str(broker.port),
                "--plant-id",
                "plant",
                "--token",
                "token",
                "--uart",
                self.slave_path,
                "--state-file",
                str(self.state_file),
                "--control-socket",
                str(self.control_path),
                "--response-timeout",
                str(response_timeout),
                "--read-gap",
                "5",
                "--write-gap",
                "5",
                "--persist",
                "1" if persist else "0",
            ],
            stdout=subprocess.DEVNULL,
            stderr=self.stderr_file,
        )
        os.close(slave)
        wait_until(lambda: self.control_path.exists() or self._raise_if_exited())

    def _raise_if_exited(self):
        if self.process.poll() is not None:
            self.stderr_file.flush()
            error = self.stderr_path.read_text(encoding="utf-8")
            raise AssertionError(
                f"host runner exited with code {self.process.returncode}:\n{error}"
            )
        return False

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        os.close(self.master)
        self.stderr_file.close()

    def answer_sequence(self, exchanges):
        first_received = threading.Event()

        def worker():
            for index, (request, response, delay) in enumerate(exchanges):
                received = bytearray()
                deadline = time.monotonic() + 5
                while len(received) < len(request) and time.monotonic() < deadline:
                    readable, _, _ = select.select([self.master], [], [], 0.2)
                    if readable:
                        received.extend(os.read(self.master, len(request) - len(received)))
                if bytes(received) != request:
                    return
                if index == 0:
                    first_received.set()
                if delay:
                    time.sleep(delay)
                if response is not None:
                    os.write(self.master, response)

        thread = threading.Thread(target=worker, daemon=True)
        thread.start()
        return thread, first_received

    def answer_once(self, response=RESPONSE):
        thread, _ = self.answer_sequence([(REQUEST, response, 0)])
        return thread

    def answer_many(self, count, response=RESPONSE):
        def worker():
            pending = bytearray()
            answered = 0
            deadline = time.monotonic() + max(10, count / 20)
            while answered < count and time.monotonic() < deadline:
                readable, _, _ = select.select([self.master], [], [], 0.2)
                if not readable:
                    continue
                pending.extend(os.read(self.master, 4096))
                while len(pending) >= len(REQUEST):
                    request = bytes(pending[: len(REQUEST)])
                    del pending[: len(REQUEST)]
                    if request != REQUEST:
                        return
                    os.write(self.master, response)
                    answered += 1

        thread = threading.Thread(target=worker, daemon=True)
        thread.start()
        return thread

    def control(self, command):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.connect(str(self.control_path))
            client.sendall((command + "\n").encode())
            return client.recv(1024).decode()


def connected_cloud(broker, client_id="host-test-cloud", include_keepalive=False):
    client = mqtt_client(client_id)
    connected = threading.Event()
    messages = []
    keepalives = []

    def on_connect(client, userdata, flags, reason_code, *args):
        if int(reason_code) == 0:
            client.subscribe("plant/ModbusInMqtt/fromDevice", qos=2)
            if include_keepalive:
                client.subscribe("plant/keepalive", qos=1)
            connected.set()

    def on_message(client, userdata, message):
        if message.topic == "plant/keepalive":
            keepalives.append((message.payload, message.qos))
        else:
            messages.append(json.loads(message.payload))

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect("127.0.0.1", broker.port, 15)
    client.loop_start()
    wait_until(connected.is_set)
    client.gbb_keepalives = keepalives
    return client, messages


def test_mqtt_to_pty_roundtrip(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path)
    client, messages = connected_cloud(broker)
    try:
        time.sleep(0.2)  # allow the host subscriber to receive its SUBACK
        inverter = host.answer_once()
        payload = {
            "OrderId": "roundtrip",
            "Lines": [{"LineNo": 0, "Modbus": REQUEST.hex().upper()}],
        }
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()
        response = wait_until(lambda: messages[0] if messages else None)
        inverter.join(timeout=2)
        assert response["OrderId"] == "roundtrip"
        assert response["Lines"][0]["Modbus"] == RESPONSE.hex().upper()
        assert response["ProtocolVersion"] == 2
        assert response["ClientEnvironment"] == "GbbDongle/host"
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


def test_modbus_timeout_is_published(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path, response_timeout=40)
    client, messages = connected_cloud(broker)
    try:
        time.sleep(0.2)
        payload = {
            "OrderId": "timeout",
            "Lines": [{"LineNo": 4, "Modbus": REQUEST.hex().upper()}],
        }
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()
        response = wait_until(lambda: messages[0] if messages else None)
        assert response["Lines"][0]["Error"] == "Response timeout"
        assert "Modbus" not in response["Lines"][0]
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


def test_multiple_read_write_and_exception_lines(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path)
    client, messages = connected_cloud(broker)
    try:
        time.sleep(0.2)
        inverter, _ = host.answer_sequence(
            [
                (REQUEST, RESPONSE, 0),
                (WRITE_REQUEST, WRITE_REQUEST, 0),
                (REQUEST, EXCEPTION_RESPONSE, 0),
            ]
        )
        payload = {
            "OrderId": "multiple",
            "Lines": [
                {"LineNo": 1, "Modbus": REQUEST.hex().upper()},
                {"LineNo": 2, "Modbus": WRITE_REQUEST.hex().upper()},
                {"LineNo": 3, "Modbus": REQUEST.hex().upper()},
            ],
        }
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()
        response = wait_until(lambda: messages[0] if messages else None)
        inverter.join(timeout=2)
        assert [line["Modbus"] for line in response["Lines"]] == [
            RESPONSE.hex().upper(),
            WRITE_REQUEST.hex().upper(),
            EXCEPTION_RESPONSE.hex().upper(),
        ]
        assert all("Error" not in line for line in response["Lines"])
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


@pytest.mark.parametrize(
    ("case", "inverter_response", "expected_error"),
    [
        ("bad-crc", RESPONSE[:-1] + bytes((RESPONSE[-1] ^ 0xFF,)), "Invalid CRC in response"),
        ("partial", RESPONSE[:4], "Response timeout"),
    ],
)
def test_invalid_inverter_responses_are_published(
    host_binary, broker, tmp_path, case, inverter_response, expected_error
):
    host = HostProcess(host_binary, broker, tmp_path, response_timeout=50)
    client, messages = connected_cloud(broker, f"invalid-{case}")
    try:
        time.sleep(0.2)
        inverter = host.answer_once(inverter_response)
        payload = {
            "OrderId": case,
            "Lines": [{"LineNo": 7, "Modbus": REQUEST.hex().upper()}],
        }
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()
        response = wait_until(lambda: messages[0] if messages else None)
        inverter.join(timeout=2)
        assert response["Lines"][0]["Error"] == expected_error
        assert "Modbus" not in response["Lines"][0]
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


def test_latest_request_replaces_queue_during_transmission(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path)
    client, messages = connected_cloud(broker)
    try:
        time.sleep(0.2)
        inverter, first_received = host.answer_sequence(
            [(REQUEST, RESPONSE, 0.2), (REQUEST, RESPONSE, 0)]
        )

        def publish(order_id):
            payload = {
                "OrderId": order_id,
                "Lines": [{"LineNo": 0, "Modbus": REQUEST.hex().upper()}],
            }
            client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()

        publish("active")
        wait_until(first_received.is_set)
        publish("replaced")
        publish("latest")
        wait_until(lambda: len(messages) == 2)
        inverter.join(timeout=2)
        assert [message["OrderId"] for message in messages] == ["active", "latest"]
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


def test_keepalive_uses_expected_topic_payload_and_qos(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path)
    client, _ = connected_cloud(broker, include_keepalive=True)
    try:
        time.sleep(0.2)
        assert host.control("MONOTONIC 0") == "OK\n"
        assert host.control("ADVANCE 60000") == "OK\n"
        payload, qos = wait_until(
            lambda: client.gbb_keepalives[0] if client.gbb_keepalives else None
        )
        assert payload == b""
        assert qos == 1
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


def test_last_log_does_not_repeat_consumed_entries(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path)
    client, messages = connected_cloud(broker)
    try:
        time.sleep(0.2)
        client.publish("plant/ModbusInMqtt/toDevice", "{", qos=1).wait_for_publish()
        time.sleep(0.1)
        payload = {"OrderId": "logs", "SendLastLog": 1, "Lines": []}
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()
        first = wait_until(lambda: messages[0] if messages else None)
        assert "Ignoring malformed toDevice payload" in first["LastLog"]

        payload["OrderId"] = "logs-empty"
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()
        second = wait_until(lambda: messages[1] if len(messages) > 1 else None)
        assert "Ignoring malformed toDevice payload" not in second["LastLog"]
        assert "Cloud -> device: received a request" in second["LastLog"]
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


def test_emergency_blob_survives_restart_and_clears_after_delivery(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path, persist=True)
    client, messages = connected_cloud(broker)
    try:
        time.sleep(0.2)
        payload = {
            "IsInvSetup": 1,
            "LinesOnNoInvSetup": [{"LineNo": 9, "Modbus": REQUEST.hex().upper()}],
            "Lines": [],
        }
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()
        wait_until(host.state_file.exists)
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()

    restarted = HostProcess(host_binary, broker, tmp_path, persist=True)
    try:
        assert restarted.control("TIME 3600") == "OK\n"
        assert restarted.control("MONOTONIC 5000") == "OK\n"
        time.sleep(0.05)
        assert restarted.control(f"TIME {2 * 3600 + 11 * 60}") == "OK\n"
        inverter = restarted.answer_once()
        assert restarted.control("ADVANCE 5000") == "OK\n"
        inverter.join(timeout=5)
        assert not inverter.is_alive()
        wait_until(lambda: not restarted.state_file.exists())
    finally:
        restarted.close()


def test_emergency_failure_retries_after_backoff(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path, response_timeout=40, persist=True)
    client, messages = connected_cloud(broker, "emergency-retry")
    try:
        assert host.control("TIME 3600") == "OK\n"
        assert host.control("MONOTONIC 0") == "OK\n"
        time.sleep(0.2)
        setup = {
            "IsInvSetup": 1,
            "LinesOnNoInvSetup": [{"LineNo": 9, "Modbus": REQUEST.hex().upper()}],
            "Lines": [],
        }
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(setup), qos=1).wait_for_publish()
        wait_until(lambda: len(messages) == 1)
        wait_until(host.state_file.exists)

        inverter, first_received = host.answer_sequence(
            [(REQUEST, None, 0), (REQUEST, RESPONSE, 0)]
        )
        assert host.control(f"TIME {2 * 3600 + 11 * 60}") == "OK\n"
        assert host.control("ADVANCE 5000") == "OK\n"
        wait_until(first_received.is_set)
        assert host.control("ADVANCE 100") == "OK\n"
        wait_until(lambda: host_stats(host)["errors"] == 1)
        assert host.control("ADVANCE 60000") == "OK\n"
        wait_until(lambda: host_stats(host)["emergency_runs"] == 2)
        inverter.join(timeout=2)
        assert not inverter.is_alive()
        wait_until(lambda: not host.state_file.exists())
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


def test_fresh_setup_cancels_emergency_and_explicit_clear_removes_blob(
    host_binary, broker, tmp_path
):
    host = HostProcess(host_binary, broker, tmp_path, persist=True)
    client, messages = connected_cloud(broker, "emergency-cancel")
    try:
        assert host.control("TIME 3600") == "OK\n"
        assert host.control("MONOTONIC 0") == "OK\n"
        time.sleep(0.2)
        setup = {
            "IsInvSetup": 1,
            "LinesOnNoInvSetup": [
                {"LineNo": 9, "Modbus": REQUEST.hex().upper()},
                {"LineNo": 10, "Modbus": REQUEST.hex().upper()},
            ],
            "Lines": [],
        }
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(setup), qos=1).wait_for_publish()
        wait_until(lambda: len(messages) == 1)
        wait_until(host.state_file.exists)

        inverter, first_received = host.answer_sequence([(REQUEST, RESPONSE, 0.2)])
        assert host.control(f"TIME {2 * 3600 + 11 * 60}") == "OK\n"
        assert host.control("ADVANCE 5000") == "OK\n"
        wait_until(first_received.is_set)
        cancel = {"OrderId": "cancel", "IsInvSetup": 1, "Lines": []}
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(cancel), qos=1).wait_for_publish()
        inverter.join(timeout=2)
        assert not inverter.is_alive()
        wait_until(lambda: any(message.get("OrderId") == "cancel" for message in messages))
        readable, _, _ = select.select([host.master], [], [], 0.15)
        assert not readable
        assert host.state_file.exists()

        clear = {"OrderId": "clear", "LinesOnNoInvSetup": [], "Lines": []}
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(clear), qos=1).wait_for_publish()
        wait_until(lambda: any(message.get("OrderId") == "clear" for message in messages))
        wait_until(lambda: not host.state_file.exists())
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()


def test_host_reconnects_after_broker_restart(host_binary, broker, tmp_path):
    host = HostProcess(host_binary, broker, tmp_path)
    first_client, _ = connected_cloud(broker, "before-restart")
    first_client.loop_stop()
    first_client.disconnect()
    broker.stop()
    time.sleep(0.2)
    broker.start()
    client, messages = connected_cloud(broker, "after-restart")
    try:
        # libmosquitto retries after one second and subscribes again in its
        # connect callback. A little margin keeps this deterministic on CI.
        time.sleep(1.5)
        inverter = host.answer_once()
        payload = {
            "OrderId": "after-restart",
            "Lines": [{"LineNo": 0, "Modbus": REQUEST.hex().upper()}],
        }
        client.publish("plant/ModbusInMqtt/toDevice", json.dumps(payload), qos=1).wait_for_publish()
        response = wait_until(lambda: messages[0] if messages else None)
        inverter.join(timeout=2)
        assert response["OrderId"] == "after-restart"
        assert response["Lines"][0]["Modbus"] == RESPONSE.hex().upper()
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()
