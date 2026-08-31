import json
import os
from pathlib import Path

import pytest

from test_e2e import Broker, HostProcess, REQUEST, connected_cloud, host_binary, wait_until


pytestmark = pytest.mark.skipif(
    os.environ.get("GBB_RUN_SOAK") != "1",
    reason="set GBB_RUN_SOAK=1 to run the host soak test",
)


def process_metrics(pid):
    proc = Path(f"/proc/{pid}")
    if not proc.is_dir():
        return None
    status = (proc / "status").read_text(encoding="utf-8")
    rss_kib = int(next(line.split()[1] for line in status.splitlines() if line.startswith("VmRSS:")))
    return {
        "rss_kib": rss_kib,
        "fds": len(list((proc / "fd").iterdir())),
        "threads": len(list((proc / "task").iterdir())),
    }


def test_repeated_roundtrips_do_not_leak(host_binary, tmp_path):
    iterations = int(os.environ.get("GBB_SOAK_ITERATIONS", "1000"))
    broker = Broker(tmp_path)
    broker.start()
    host = HostProcess(host_binary, broker, tmp_path)
    client, messages = connected_cloud(broker)
    inverter = host.answer_many(iterations)
    try:
        payload = json.dumps({"Lines": [{"LineNo": 0, "Modbus": REQUEST.hex().upper()}]})
        baseline = process_metrics(host.process.pid)
        for index in range(iterations):
            client.publish("plant/ModbusInMqtt/toDevice", payload, qos=1).wait_for_publish()
            wait_until(lambda: len(messages) >= index + 1, timeout=10)
        inverter.join(timeout=10)
        assert len(messages) == iterations
        final = process_metrics(host.process.pid)
        if baseline is not None and final is not None:
            assert final["rss_kib"] <= baseline["rss_kib"] + 4096
            assert final["fds"] <= baseline["fds"] + 2
            assert final["threads"] == baseline["threads"]
    finally:
        client.loop_stop()
        client.disconnect()
        host.close()
        broker.stop()
