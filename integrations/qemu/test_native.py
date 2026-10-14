#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the actual QEMU PCI device through qtest/QMP, without a guest OS."""
import argparse
import json
import os
import pathlib
import socket
import struct
import subprocess
import tempfile
import time


def dwords(*values):
    return struct.pack('<' + 'I' * len(values), *values)


def packet(command, *values):
    payload = dwords(*values)
    return dwords(command, len(payload)) + payload


class TestMachine:
    def __init__(self, binary, directory, kernel=None):
        self.log = open(directory / 'qemu.log', 'w')
        args = [
            binary, '-machine', 'q35', '-accel', 'tcg', '-m', '64',
            '-nodefaults', '-display', 'none', '-S',
            '-device', 'vmware-svga,vlkn=on,addr=02.0',
            '-qtest', f'unix:{directory}/qtest.sock,server=on,wait=off',
            '-qmp', f'unix:{directory}/qmp.sock,server=on,wait=off',
        ]
        if kernel:
            args.remove('-S')
            args += ['-kernel', os.path.abspath(kernel), '-debugcon', f'file:{directory}/guest.log',
                     '-device', 'isa-debug-exit,iobase=0xf4,iosize=4']
        self.proc = subprocess.Popen(args, stdout=self.log, stderr=self.log)
        self.qtest = self.connect(directory / 'qtest.sock')
        self.qmp = self.connect(directory / 'qmp.sock')
        self.qmp.readline()
        self.command('qmp_capabilities')

    def connect(self, path):
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            try:
                sock = socket.socket(socket.AF_UNIX)
                sock.settimeout(20)
                sock.connect(str(path))
                return sock.makefile('rwb', buffering=0)
            except (FileNotFoundError, ConnectionRefusedError):
                sock.close()
                if self.proc.poll() is not None:
                    raise RuntimeError('QEMU exited; inspect qemu.log')
                time.sleep(0.05)
        raise TimeoutError(f'QEMU socket unavailable: {path}')

    def command(self, name, arguments=None, allow_error=False):
        request = {'execute': name}
        if arguments is not None: request['arguments'] = arguments
        self.qmp.write(json.dumps(request).encode() + b'\n')
        while True:
            result = json.loads(self.qmp.readline())
            if 'event' not in result:
                if allow_error and 'error' in result: return result
                assert 'return' in result, result
                return result['return']

    def qt(self, text):
        self.qtest.write(text.encode() + b'\n')
        response = self.qtest.readline().decode().strip()
        assert response.startswith('OK'), (text, response)
        return response[2:].strip()

    def write(self, addr, data):
        self.qt(f'write {addr:#x} {len(data):#x} 0x{data.hex()}')

    def read(self, addr, length):
        return bytes.fromhex(self.qt(f'read {addr:#x} {length:#x}').removeprefix('0x'))

    def cfg(self, offset, value=None):
        self.qt(f'outl 0xcf8 {0x80001000 + offset:#x}')
        if value is None:
            return int(self.qt('inl 0xcfc'), 0)
        self.qt(f'outl 0xcfc {value:#x}')

    def reg(self, index, value=None):
        self.qt(f'outl 0xc000 {index:#x}')
        if value is None:
            return int(self.qt('inl 0xc001'), 0)
        self.qt(f'outl 0xc001 {value:#x}')

    def configure(self):
        assert self.cfg(0) == 0x040515ad
        self.cfg(0x10, 0xc001)
        self.cfg(0x14, 0xe0000000)
        self.cfg(0x18, 0xf0000000)
        self.cfg(4, 7)
        self.reg(0, 0x90000002)
        self.reg(2, 16)
        self.reg(3, 16)
        self.reg(7, 32)
        self.reg(1, 1)
        assert self.reg(17) & (1 << 14), 'SVGA_CAP_3D missing'
        assert self.read(0xf000001c, 4) == dwords(0x20001), 'Mesa legacy protocol negotiation requires WS8_B1'
        length = struct.unpack('<I', self.read(0xf0000080, 4))[0]
        words = struct.unpack('<' + 'I' * (length - 2), self.read(0xf0000088, (length - 2) * 4))
        caps = dict(zip(words[::2], words[1::2]))
        assert caps[17] == 0x3f800000, 'MAX_POINT_SIZE must encode FLOAT 1.0'
        size = self.reg(19)
        self.write(0xf0000000, dwords(4096, size, 4096, 4096))
        self.reg(20, 1)
        return size

    def submit(self, data, start=4096):
        size = self.reg(19)
        tail = min(len(data), size - start)
        self.write(0xf0000000 + start, data[:tail])
        if tail < len(data):
            self.write(0xf0001000, data[tail:])
        end = 4096 + (start - 4096 + len(data)) % (size - 4096)
        self.write(0xf0000008, dwords(end, start))
        self.reg(21, 1)
        return end

    def close(self):
        if self.proc.poll() is None:
            try:
                self.command('quit')
                self.proc.wait(timeout=10)
            except Exception:
                self.proc.kill()
                self.proc.wait()
        self.log.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('qemu')
    parser.add_argument('--output', required=True)
    parser.add_argument('--kernel')
    args = parser.parse_args()
    output = pathlib.Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / 'result.json').write_text(json.dumps({'passed': False}) + '\n')
    with tempfile.TemporaryDirectory(prefix='vlkn-qtest-') as temp:
        directory = pathlib.Path(temp)
        machine = TestMachine(os.path.abspath(args.qemu), directory)
        try:
            size = machine.configure()
            wire = packet(1040, 1, 1 << 6, 2, 1, 0, 0, 0, 0, 0, 16, 16, 1)
            wire += packet(1045, 1)
            wire += packet(1050, 1, 2, 1, 0, 0)
            wire += packet(1057, 1, 1, 0xff2468ac, 0x3f800000, 0, 0, 0, 16, 16)
            wire += packet(1058, 1, 0, 0, 16, 16, 0, 0)
            wire += dwords(30, 77)
            end = machine.submit(wire)
            assert machine.read(0xf000000c, 4) == dwords(end)
            assert machine.read(0xf0000018, 4) == dwords(77)
            assert machine.read(0xe0000000, 16 * 16 * 4) == dwords(0xff2468ac) * 256
            # Read back through a GMR backed by actual QEMU guest RAM.
            wire = dwords(41, 3, 1, 42, 3, 0, 0, 1, 0x200)
            wire += packet(1044, 3, 0, 64, 1, 0, 0, 2, 0, 0, 0, 16, 16, 1, 0, 0, 0)
            wire += dwords(30, 81)
            machine.submit(wire)
            assert machine.read(0x200000, 1024) == dwords(0xff2468ac) * 256
            # Upload from the same GMR and check presentation reads the new data.
            machine.write(0x200000, dwords(0xffabcdef) * 256)
            wire = packet(1044, 3, 0, 64, 1, 0, 0, 1, 0, 0, 0, 16, 16, 1, 0, 0, 0)
            wire += packet(1058, 1, 0, 0, 16, 16, 0, 0) + dwords(30, 82)
            machine.submit(wire)
            assert machine.read(0xe0000000, 1024) == dwords(0xffabcdef) * 256
            # Wrap a fence across the ring boundary.
            end = machine.submit(dwords(30, 78), start=size - 4)
            assert machine.read(0xf000000c, 4) == dwords(end)
            assert machine.read(0xf0000018, 4) == dwords(78)
            # An incomplete packet must remain unconsumed until its tail arrives.
            machine.submit(dwords(30))
            assert machine.read(0xf000000c, 4) == dwords(4096)
            machine.write(0xf0001004, dwords(79))
            machine.write(0xf0000008, dwords(4104))
            machine.reg(21, 1)
            assert machine.read(0xf0000018, 4) == dwords(79)
            # Native FIFO register storage must not overlap command payloads.
            machine.write(0xf0000000, dwords(2048, size, 2056, 2048))
            machine.write(0xf0000800, dwords(30, 0xbad))
            machine.reg(21, 1)
            assert machine.read(0xf000000c, 4) == dwords(2048)
            assert machine.read(0xf0000018, 4) == dwords(79)
            machine.command('system_reset')
            machine.cfg(0x10, 0xc001)
            machine.cfg(4, 7)
            assert machine.reg(1) == 0
            machine.configure()
            end = machine.submit(dwords(30, 80))
            assert machine.read(0xf0000018, 4) == dwords(80)
            # Migration must fail explicitly until renderer state is serializable.
            status = machine.command('migrate', {'uri': 'exec:cat >/dev/null'}, allow_error=True)
            assert 'error' in status and 'SVGA3D' in status['error']['desc'], status
            print('Native QEMU PCI/FIFO, Vulkan pixels, wrapped/incomplete fences and reset: PASS')
            (output / 'result.json').write_text(json.dumps({'passed': False, 'device_tests_passed': True, 'scope': 'qtest device integration; no guest OS boot', 'migration': status}, indent=2) + '\n')
        finally:
            machine.close()
            (output / 'qemu.log').write_bytes((directory / 'qemu.log').read_bytes())
    assert machine.proc.returncode == 0, f'QEMU teardown exited {machine.proc.returncode}'
    if args.kernel:
        with tempfile.TemporaryDirectory(prefix='vlkn-guest-') as temp:
            directory = pathlib.Path(temp)
            machine = TestMachine(os.path.abspath(args.qemu), directory, args.kernel)
            try:
                for boots in (1, 2):
                    deadline = time.monotonic() + 30
                    while time.monotonic() < deadline:
                        log = directory / 'guest.log'
                        content = log.read_text() if log.exists() else ''
                        if 'GUEST_FAIL' in content or machine.proc.poll() is not None:
                            raise RuntimeError('Bare-metal guest failed; inspect guest.log')
                        if content.count('GUEST_BOOT_PASS') >= boots:
                            break
                        time.sleep(0.05)
                    else:
                        raise TimeoutError('Bare-metal guest did not complete')
                    if boots == 1:
                        # Reboot must repaint, rather than pass on retained VRAM.
                        machine.write(0xe0000000, dwords(0xff000000) * 256)
                        assert machine.read(0xe0000000, 1024) == dwords(0xff000000) * 256
                        machine.command('system_reset')
                print('Bare-metal guest cold boot and reboot with Vulkan framebuffer pixels: PASS')
                result = json.loads((output / 'result.json').read_text())
                result['guest_cold_boot_and_reset'] = True
                (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            finally:
                machine.close()
                (output / 'guest-qemu.log').write_bytes((directory / 'qemu.log').read_bytes())
                if (directory / 'guest.log').exists():
                    (output / 'guest.log').write_bytes((directory / 'guest.log').read_bytes())
        assert machine.proc.returncode == 0, f'Guest QEMU teardown exited {machine.proc.returncode}'
    logs = [output / 'qemu.log']
    if args.kernel: logs.append(output / 'guest-qemu.log')
    for path in logs:
        log = path.read_text()
        assert '[Vulkan Validation ERROR]' not in log and '[Vulkan Validation WARN]' not in log, str(path)
    result = json.loads((output / 'result.json').read_text())
    result['passed'] = True
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
