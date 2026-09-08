#!/usr/bin/env python3
import os
import pathlib
import re
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
VERSION_H = ROOT / "include" / "version.h"


def read_version():
    text = VERSION_H.read_text(encoding='utf-8')
    match = re.search(r'^#define\s+ORION_VERSION\s+"([^"]+)"', text, re.MULTILINE)
    if not match:
        raise SystemExit('ORION_VERSION missing from include/version.h')
    return match.group(1)


VERSION = read_version()
ISO = BUILD / f"UN_Orion-v{VERSION}-install.iso"
KERNEL_BANNER = f"UN_Orion {VERSION} alive"


def find_ovmf(env_name, names):
    value = os.environ.get(env_name)
    if value and pathlib.Path(value).exists():
        return pathlib.Path(value)
    for root in (pathlib.Path('/usr/share/OVMF'), pathlib.Path('/usr/share/edk2/ovmf')):
        for name in names:
            path = root / name
            if path.exists():
                return path
    raise SystemExit(f"{env_name} firmware not found")


CODE = find_ovmf('OVMF_CODE', ['OVMF_CODE_4M.fd', 'OVMF_CODE.fd'])
VARS = find_ovmf('OVMF_VARS', ['OVMF_VARS_4M.fd', 'OVMF_VARS.fd'])


def wait_text(path, needle, timeout, proc=None):
    deadline = time.time() + timeout
    while time.time() < deadline:
        text = path.read_text(errors='ignore') if path.exists() else ''
        if needle in text:
            return text
        if proc is not None and proc.poll() is not None:
            raise RuntimeError(f"QEMU exited while waiting for {needle!r}:\n{text[-2000:]}")
        time.sleep(0.08)
    text = path.read_text(errors='ignore') if path.exists() else ''
    raise RuntimeError(f"timeout waiting for {needle!r}:\n{text[-2000:]}")


def hmp(sock_path, command):
    deadline = time.time() + 3
    while time.time() < deadline:
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(str(sock_path))
            break
        except OSError:
            time.sleep(0.05)
    else:
        raise RuntimeError('QEMU monitor socket did not appear')
    try:
        sock.settimeout(1)
        try:
            sock.recv(4096)
        except OSError:
            pass
        sock.sendall((command + '\n').encode())
        time.sleep(0.05)
    finally:
        sock.close()


def stop(proc):
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2)


if not ISO.exists():
    raise SystemExit(f'install ISO missing: {ISO.name}; run make iso first')

with tempfile.TemporaryDirectory(prefix='orion-install-') as td:
    td = pathlib.Path(td)
    target = td / 'target.img'
    with target.open('wb') as fp:
        fp.truncate(128 * 1024 * 1024)

    vars_install = td / 'vars-install.fd'
    shutil.copyfile(VARS, vars_install)
    install_log = td / 'install.log'
    monitor = td / 'monitor.sock'

    install_cmd = [
        'qemu-system-x86_64', '-machine', 'q35', '-m', '256M',
        '-drive', f'if=pflash,format=raw,readonly=on,file={CODE}',
        '-drive', f'if=pflash,format=raw,file={vars_install}',
        '-drive', f'file={target},if=virtio,format=raw',
        '-cdrom', str(ISO), '-boot', 'd',
        '-nic', 'none', '-display', 'none',
        '-serial', f'file:{install_log}',
        '-monitor', f'unix:{monitor},server,nowait',
    ]
    proc = subprocess.Popen(install_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        wait_text(install_log, 'Press I within 5 seconds', 12, proc)
        hmp(monitor, 'sendkey i')
        wait_text(install_log, 'Press Y within 10 seconds', 5, proc)
        hmp(monitor, 'sendkey y')
        wait_text(install_log, 'UN_Orion install complete', 20, proc)
    finally:
        stop(proc)

    vars_disk = td / 'vars-disk.fd'
    shutil.copyfile(VARS, vars_disk)
    disk_log = td / 'disk.log'
    disk_cmd = [
        'qemu-system-x86_64', '-machine', 'q35', '-m', '256M',
        '-drive', f'if=pflash,format=raw,readonly=on,file={CODE}',
        '-drive', f'if=pflash,format=raw,file={vars_disk}',
        '-drive', f'file={target},if=virtio,format=raw',
        '-boot', 'c', '-nic', 'none', '-display', 'none', '-monitor', 'none',
        '-serial', f'file:{disk_log}',
    ]
    proc = subprocess.Popen(disk_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        wait_text(disk_log, KERNEL_BANNER, 12, proc)
        wait_text(disk_log, 'Orion desktop ready', 5, proc)
    finally:
        stop(proc)

print(f'UN_Orion {VERSION} installer smoke passed: ISO -> writable disk -> disk-only UEFI boot')
