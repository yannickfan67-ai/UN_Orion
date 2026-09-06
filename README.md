# UN_Orion

UN_Orion is a from-scratch x86_64 UEFI operating system with its own framebuffer desktop.

## Desktop
- native framebuffer desktop using Traf Typeface v2.1 / 2.100
- taskbar + Start menu
- PS/2 mouse (IRQ12), real cursor, movable/focusable windows
- Terminal, Files, Notes, Paint, Network, About
- **UN_Vela 0.1.0**, the native UN_Orion browser
- **Aster Engine 0.1.0**, UN_Vela's from-scratch browser engine

## UN_Vela
UN_Vela is the formal name of UN_Orion's browser. It is not a Chromium/WebKit/Gecko port.

Current browser shell features:
- native Orion window and address bar
- HTTP navigation through the Orion network stack
- Aster-rendered start page and document viewport
- Traf Typeface rendering
- engine/version identity shown directly in the browser UI

## Aster Engine
Aster is the browser engine created specifically for UN_Vela.

Aster 0.1 currently contains:
- HTML tokenizer/parser
- fixed-capacity DOM tree
- common element recognition (`html`, `head`, `body`, `title`, `h1`, `h2`, `p`, `div`, `br`, `a`, lists, emphasis and code)
- entity decoding
- link metadata
- block/inline layout
- line wrapping
- paint list generation
- native framebuffer painting through UN_Orion graphics APIs

The engine lives in `kernel/aster.c` with its public ABI in `include/aster.h`. UN_Vela is separated into `kernel/vela.c` / `include/vela.h`.

## Networking
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

Terminal commands include `net`, `ping`, `vela`, `browser`, `nettest`, and `openwrt`.
`openwrt` switches the early static profile to `192.168.1.2/24`, gateway/DNS `192.168.1.1`.

## OpenWrt lab
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
vela
```
and browse `192.168.1.1/` if the OpenWrt image exposes an HTTP UI.

## Build
```bash
sudo apt install clang lld llvm make qemu-system-x86 ovmf gnu-efi
make
make run
```

## Current limitations
- IPv4 configuration is static; DHCP is a future network milestone.
- TCP is a small synchronous client, not yet a general socket API.
- HTTP only; TLS/HTTPS is not implemented yet.
- Aster does not yet implement CSS, JavaScript, images, forms or a full HTML5 tree builder.
- The current HTTP path still contains some legacy text-normalization behavior; moving all markup handling exclusively into Aster is the next browser-engine cleanup.
- Files/Notes persistence and ORX loading from disk are still in progress.

Stable desktop release: `v0.0.4`. Current `main` contains the network stack plus the first UN_Vela/Aster integration.
