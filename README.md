# UN_Orion

UN_Orion is a from-scratch x86_64 UEFI operating system with its own framebuffer desktop.

## Current development status — v0.0.5

### Desktop
- native framebuffer desktop using Traf Typeface v2.1 / 2.100
- taskbar + Start menu
- PS/2 mouse (IRQ12), real cursor, movable/focusable windows
- Terminal, Files, Notes, Paint, About
- **Orion Browser** with address bar and minimal HTML text rendering
- **Network** control panel with live adapter/IP/gateway/packet counters and gateway ping

### Networking
The first network stack is intentionally small but real:
- PCI enumeration
- RTL8139 driver (PIO + DMA rings, polling RX)
- Ethernet II
- ARP
- IPv4
- ICMP echo
- UDP
- DNS A lookup
- minimal TCP client
- HTTP/1.0 GET

Default QEMU user-network profile:
- guest `10.0.2.15/24`
- gateway `10.0.2.2`
- DNS `10.0.2.3`

Terminal commands include `net`, `ping`, `browser`, `nettest`, and `openwrt`.
`openwrt` switches the early static profile to `192.168.1.2/24`, gateway/DNS `192.168.1.1`.

### OpenWrt lab
`scripts/run-openwrt-lab.sh` wires two QEMU machines without TAP/root networking:

`Internet <- QEMU user NAT <- OpenWrt WAN | OpenWrt LAN <- socket LAN -> UN_Orion`

Usage:
```bash
make all
OPENWRT_IMAGE=/path/to/openwrt-x86-64.img ./scripts/run-openwrt-lab.sh
```
Then in Orion Terminal:
```text
openwrt
ping
browser
```
and browse `192.168.1.1/` if the OpenWrt image exposes an HTTP UI.

## Build
```bash
sudo apt install clang lld llvm make qemu-system-x86 ovmf gnu-efi
make
make run
```

## Current limitations
- IPv4 configuration is static; DHCP is the next network milestone.
- TCP is a small synchronous client, not yet a general socket API.
- HTTP only; TLS/HTTPS is not implemented yet.
- HTML rendering is text-oriented; CSS/JS/images are not implemented yet.
- Files/Notes persistence and ORX loading from disk are still in progress.

Stable desktop release: `v0.0.4`. Current `main` is the v0.0.5 network/browser development line.
