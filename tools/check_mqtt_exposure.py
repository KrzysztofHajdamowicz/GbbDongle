#!/usr/bin/env python3
"""Fail when a firmware config would expose an entity over MQTT or build the
cloud link without TLS.

The mqtt: client is the GbbOptimizer cloud link. ESPHome mirrors every
non-internal entity to <device_name>/<domain>/<id>/state and subscribes to
its .../command topic, which would hand the cloud broker the Plant Token and
control over the device (see the mqtt: block in firmware/common/base.yaml).
Every entity therefore needs `internal: true` or an empty `state_topic:`.

The link uses TLS exactly when gbb_dongle: carries a certificate_authority
(there is no runtime switch), so a published config without one would send
the Plant Token in plain text. Only local bench builds drop it.

Usage: uv run python tools/check_mqtt_exposure.py firmware/<config>.yaml [...]
Runs `esphome config` on each file (with the working-tree component, like
CI) and checks the validated output, so entities pulled in through packages
and board files are covered.
"""

import subprocess
import sys
from pathlib import Path

import yaml


class _AnyTagLoader(yaml.SafeLoader):
    """SafeLoader that keeps ESPHome's custom tags (!lambda, !secret) as plain values."""


def _construct_any(loader, _suffix, node):
    if isinstance(node, yaml.ScalarNode):
        return loader.construct_scalar(node)
    if isinstance(node, yaml.SequenceNode):
        return loader.construct_sequence(node)
    return loader.construct_mapping(node)


_AnyTagLoader.add_multi_constructor("!", _construct_any)


def _exposed_entities(node, path):
    # Every validated entity config carries disabled_by_default; that is what
    # tells an entity apart from other nested config dicts.
    if isinstance(node, dict):
        if "disabled_by_default" in node:
            internal = node.get("internal") is True
            hidden = node.get("state_topic", None) == "" or node.get("command_topic", None) == ""
            if not internal and not hidden:
                yield path, node.get("name", "<unnamed>")
        for key, value in node.items():
            yield from _exposed_entities(value, f"{path}.{key}" if path else str(key))
    elif isinstance(node, list):
        for index, item in enumerate(node):
            yield from _exposed_entities(item, f"{path}[{index}]")


def check(config_path: Path) -> list[str]:
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "-s", "external_components_source", "../components", "config", str(config_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(f"{config_path}: esphome config failed")
    config = yaml.load(result.stdout, Loader=_AnyTagLoader)
    if "mqtt" not in config:
        return []
    problems = [
        f"{config_path}: {path} ({name}) exposed over MQTT, add an empty `state_topic:`"
        for path, name in _exposed_entities(config, "")
    ]
    gbb = config.get("gbb_dongle")
    if isinstance(gbb, dict) and not gbb.get("certificate_authority"):
        problems.append(f"{config_path}: gbb_dongle has no certificate_authority, the cloud link would run without TLS")
    return problems


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__, file=sys.stderr)
        return 2
    problems = []
    for arg in argv:
        problems += check(Path(arg))
    if problems:
        print("Cloud MQTT link problems:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1
    print(f"No entity exposed over MQTT and TLS on in {len(argv)} config(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
