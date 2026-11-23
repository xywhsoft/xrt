"""Select matched Linux CPU roles for the Redis and MQTT comparison jobs."""

import argparse
import os
from pathlib import Path


def placement(product, groups):
    if product == "xredis":
        if len(groups) >= 3:
            server, first, second = (group[0] for group in groups[:3])
            return f"{server},{first},{second}", "separate-cores"
        if len(groups) == 2:
            server, clients = sorted(groups, key=len)
            if len(clients) >= 2:
                return f"{server[0]},{clients[0]},{clients[1]}", "two-core-smt"
        raise ValueError("Redis comparison needs a server CPU and two client CPUs across at least two cores")

    if len(groups) >= 4:
        broker, first, second, peer = (group[0] for group in groups[:4])
        return broker, f"{first},{second}", peer, "separate-cores"
    if len(groups) == 3:
        broker, first, second = sorted(groups, key=len, reverse=True)
        if len(broker) >= 2:
            return broker[0], f"{first[0]},{second[0]}", broker[1], "three-core-smt"
    if len(groups) == 2 and all(len(group) >= 2 for group in groups):
        # Keep the receiver CPUs on a different core from the broker and peer.
        broker, clients = groups
        return broker[0], f"{clients[0]},{clients[1]}", broker[1], "two-core-smt"
    raise ValueError("MQTT comparisons need four usable logical CPUs across at least two cores")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("product", choices=("xredis", "xmqtt"))
    parser.add_argument("topology", type=Path, help="lscpu -p=CPU,CORE,SOCKET output")
    args = parser.parse_args()

    available = os.sched_getaffinity(0)
    cores = {}
    for row in args.topology.read_text(encoding="ascii").splitlines():
        if row and not row.startswith("#"):
            cpu, core, socket = map(int, row.split(","))
            if cpu in available:
                cores.setdefault((socket, core), []).append(cpu)
    try:
        print(*placement(args.product, list(cores.values())))
    except ValueError as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
