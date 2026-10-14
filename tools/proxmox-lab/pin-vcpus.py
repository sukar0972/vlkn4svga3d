#!/usr/bin/env python3
"""Apply the measured thread affinity to one running Proxmox VM only."""
import argparse
import json
import os
from pathlib import Path
import socket

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('vmid', type=int)
parser.add_argument('cpus', type=int, nargs='+', help='one host CPU per guest vCPU')
args = parser.parse_args()
if args.vmid <= 0 or len(set(args.cpus)) != len(args.cpus):
    parser.error('require a positive VM ID and distinct host CPUs')
allowed = os.sched_getaffinity(0)
if not set(args.cpus) <= allowed:
    parser.error('requested CPUs are outside the caller affinity')
pid = int(Path(f'/var/run/qemu-server/{args.vmid}.pid').read_text())
with socket.socket(socket.AF_UNIX) as sock:
    sock.settimeout(10)
    sock.connect(f'/var/run/qemu-server/{args.vmid}.qmp')
    with sock.makefile('rwb') as stream:
        greeting = json.loads(stream.readline())
        if 'QMP' not in greeting:
            raise RuntimeError('not a QMP connection')
        def command(name):
            stream.write((json.dumps({'execute': name}) + '\n').encode())
            stream.flush()
            while True:
                reply = json.loads(stream.readline())
                if 'return' in reply:
                    return reply['return']
                if 'error' in reply:
                    raise RuntimeError(reply['error'])
        command('qmp_capabilities')
        vcpus = command('query-cpus-fast')
        if sorted(cpu['cpu-index'] for cpu in vcpus) != list(range(len(args.cpus))):
            raise RuntimeError('host CPU list does not match guest vCPU count')
        tasks = Path(f'/proc/{pid}/task')
        for cpu in vcpus:
            if not (tasks / str(cpu['thread-id'])).is_dir():
                raise RuntimeError('QMP thread does not belong to this VM')
        for thread in tasks.iterdir():
            os.sched_setaffinity(int(thread.name), allowed)
        for cpu in vcpus:
            os.sched_setaffinity(cpu['thread-id'], {args.cpus[cpu['cpu-index']]})
        print(json.dumps(vcpus, indent=2))
