# UN_Orion

UN_Orion is a from-scratch x86_64 UEFI operating system with its own framebuffer desktop and an i686 Legacy BIOS compatibility path.

## Desktop
- native framebuffer desktop using Traf Typeface v2.1 / 2.100
- taskbar + Start menu
- PS/2 mouse (IRQ12), real cursor, movable/focusable windows
- IntelliMouse wheel negotiation with automatic 3-byte PS/2 fallback
- Terminal, Files, Notes, Paint, Network, About
- **UN_Vela 0.3.2-dev**, the native UN_Orion browser shell
- **Aster Engine 0.3.2**, UN_Vela's from-scratch browser engine

## UN_Vela
UN_Vela is UN_Orion's primary browser. It is not a Chromium/WebKit/Gecko port.

Current Orion browser integration includes:
- native Orion window and address bar
- Back, Forward, Reload and Go controls
- portable navigation-action ABI (Vela API 1.4)
- clickable document links with relative/root/scheme-relative URL resolution
- mouse-wheel scrolling with viewport-aware clamping
- HTML markup delivery through the Vela Platform ABI
- Aster 0.3.2 document/rendering integration
- Orion-native HTTP, HTTPS and binary-resource carrier
- lightweight CSS support through Aster 0.3.2
- safe JavaScript subset in the full profile
- BMP 24/32-bit and PPM P6 image resources with framebuffer scaling
- navigation history and local HTML documents
- engine/version identity shown directly in the browser UI

### Browser content pipeline

`network carrier -> HTTP or verified TLS 1.2 -> UN_Vela 0.3.2-dev -> Aster 0.3.2 -> Orion framebuffer`

The transport keeps HTML markup intact instead of flattening it into text. The HTTP layer requests identity encoding, accepts CRLF or LF-only header boundaries, handles Content-Length and chunked bodies, follows bounded redirects, and keeps HTTP status information separate from page markup.

### Native TLS / HTTPS

UN_Orion 0.0.8 adds an in-kernel HTTPS client built around a freestanding BearSSL 0.6 integration. The native carrier:
- negotiates TLS 1.2 and does not silently downgrade HTTPS to plaintext HTTP
- validates certificate chains against an embedded CA trust store generated at build time
- passes the requested host name to BearSSL for SNI/name validation
- validates certificate validity periods using the machine RTC
- seeds the TLS engine from x86 RDRAND and fails closed when a suitable entropy source is unavailable
- supports HTTP -> HTTPS redirects while explicitly refusing HTTPS -> HTTP redirects
- keeps TLS and TCP inside the native Orion networking path rather than proxying through the build/host OS

The default build generates trust anchors from `/etc/ssl/certs/ca-certificates.crt`; override `TLS_CA_BUNDLE=/path/to/ca-bundle.pem` when building a controlled image. BearSSL 0.6's minimal verifier handles normal DNS-name certificate validation; literal-IP certificate SAN handling is not intended to be browser-complete.

TLS 1.3, OCSP/CRL handling and a general-purpose kernel entropy subsystem are future work. The current HTTPS path deliberately fails closed instead of disabling certificate or host validation.

Hosted UN_Vela carriers continue to use the host operating system's validated TLS stack: WinHTTP on Windows and libcurl with peer/hostname verification on Linux/macOS.

### JavaScript profile
A compact, intentionally non-web-complete JavaScript subset is available in the full Vela profile. It currently recognizes controlled operations such as `document.title`, `document.body.innerHTML`, `document.write`, `console.log`, and location navigation. The separate Orion recovery-browser shell uses `VELA_PROFILE_LITE`, which disables JavaScript while sharing the same HTML/CSS/layout/history core.

## Aster Engine
Aster 0.3.2 is the rendering engine used by UN_Vela. The integrated copy follows the standalone `Aster-Engine` public ABI.

Current Aster features include:
- fixed-capacity HTML tokenizer/parser and DOM tree
- common semantic elements, headings, paragraphs, links, lists and emphasis
- CSS selectors: `*`, tag, `.class`, `#id`, `tag.class`, `tag#id`, compound class selectors, and comma-separated simple selector lists
- multi-class token matching and compact ID/class/tag specificity for color and size cascade
- parent-element CSS propagation into child text, plus inline style handling
- `color`, bold weight, size scaling, underline and `display:none`
- `<img>` layout items and host image rendering callback
- entity decoding and script/style source suppression
- link metadata and scroll-aware hit testing
- block/inline layout, line wrapping and document-height metadata
- native framebuffer painting through UN_Orion graphics APIs

Descendant, child, attribute and pseudo selectors remain intentionally unsupported in this lightweight CSS engine.

The engine lives in `kernel/aster.c` with its public ABI in `include/aster.h`. UN_Vela is separated into `kernel/vela.c` / `include/vela.h`, with the host boundary in `include/vela_platform.h`.

## Networking
The early network stack is intentionally small but real. L2/L3/L4 is separated from the NIC driver layer:
- generic PCI enumeration plus a `netdev` device ABI
- RTL8139 regression driver
- AMD PCnet family: Am79C970A / Am79C973
- Intel PRO/1000 family: 82540EM / 82543GC / 82545EM
- Intel 82583V detection through the e1000e-family path
- legacy/transitional virtio-net
- Ethernet II / ARP / IPv4
- ICMP echo
- UDP and DNS A lookup
- synchronous TCP client
- HTTP GET with redirects and body-length validation
- verified TLS 1.2 / HTTPS GET
- lightweight binary HTTP resource path for browser images

Default QEMU user-network profile:
- guest `10.0.2.15/24`
- gateway `10.0.2.2`
- DNS `10.0.2.3`

Terminal commands include `net`, `ping`, `vela`, `browser`, `nettest`, and `openwrt`.
`openwrt` switches the early static profile to `192.168.1.2/24`, gateway/DNS `192.168.1.1`.

## 86Box late-era compatibility profile
A tested late 86Box profile is included at `compat/86box/cuv4xls-c3-733.cfg`:
- ASUS CUV4X-LS / VIA Apollo Pro 133A
- VIA C3 Samuel 733 MHz
- 256 MiB SDRAM
- Voodoo3 3500 AGP
- PS/2 input, SB16 and PCnet-FAST III

The i686 Legacy BIOS bootstrap has reached protected mode on this profile with 86Box build 9001 and the 6.0 ROM set. The browser/runtime build continues to compile and boot on the i686 compatibility path. HTTPS additionally requires a CPU exposing the entropy capability used by the current TLS implementation.

## UEFI install / live media
UN_Orion has an El Torito UEFI ISO built directly from the bootable FAT system image.

```bash
make iso
```

Output:
```text
build/UN_Orion-v0.0.8-install.iso
```

`make iso-smoke` boots the ISO as a virtual DVD through OVMF and requires the bootloader, kernel and desktop to reach a healthy state.

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

## Related UN repositories
- `UN_Vela` — portable browser shell and reference Platform ABI implementation
- `Aster-Engine` — rendering engine
- `Orion-Browser` — lightweight recovery shell over Vela Lite profile
- `Orion-Executable-Tools` — ORX application SDK/toolchain
- `Orion-Driver-Interface` — technical driver ABI/package specification
- `Orion-Driver-Kit` — ODK tooling and reference drivers
- `UN_Cygnus` — from-scratch VM/hypervisor project

## Build
```bash
sudo apt install clang lld llvm make qemu-system-x86 ovmf gnu-efi ca-certificates
make
make run
```

BearSSL is fetched reproducibly by the project build helper; the CA bundle is converted into freestanding trust-anchor data during the build.

## Validation
The main CI builds boot media and exercises:
- Aster selector/cascade host smoke
- x86_64 UEFI QEMU boot
- i686 Legacy BIOS full-system boot
- UEFI install ISO boot
- end-to-end disk installation
- self-contained QEMU HTTP -> HTTPS redirect with a temporary CI CA
- TLS 1.2 certificate chain/name/time validation inside UN_Orion
- HTTPS markup delivery through UN_Vela -> Aster title parsing

## Current limitations
- IPv4 configuration is static; DHCP is a future network milestone.
- TCP is a small synchronous client, not yet a general socket API.
- Native TLS currently targets TLS 1.2 and uses x86 RDRAND as its fail-closed entropy source.
- BearSSL's lightweight verifier is not a full modern-browser PKI implementation.
- CSS and JavaScript are deliberately lightweight subsets, not web-platform conformance implementations.
- Native image decoding currently targets lightweight BMP/PPM formats rather than the full modern web image set.
- Files/Notes persistence and ORX loading from disk are still in progress.

Stable desktop release: `v0.0.4`. The current `v0.0.8` development line adds validated native HTTPS on top of networking, multi-firmware compatibility, install media, UN_Vela/Aster 0.3.x, images, CSS/JS subsets and browser interaction work.
