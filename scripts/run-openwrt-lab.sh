#!/usr/bin/env bash
set -euo pipefail

: "${OPENWRT_IMAGE:?Set OPENWRT_IMAGE to an OpenWrt x86_64 disk image}"
QEMU=${QEMU:-qemu-system-x86_64}
ORION_IMG=${ORION_IMG:-build/orion.img}
OVMF_CODE=${OVMF_CODE:-$(find /usr/share/OVMF -maxdepth 1 -type f \( -name 'OVMF_CODE_4M.fd' -o -name 'OVMF_CODE.fd' \) | head -n1)}
OVMF_VARS_TEMPLATE=${OVMF_VARS:-$(find /usr/share/OVMF -maxdepth 1 -type f \( -name 'OVMF_VARS_4M.fd' -o -name 'OVMF_VARS.fd' \) | head -n1)}

[[ -f "$OPENWRT_IMAGE" ]] || { echo "OpenWrt image not found: $OPENWRT_IMAGE" >&2; exit 1; }
[[ -f "$ORION_IMG" ]] || { echo "UN_Orion image not found: $ORION_IMG (run make first)" >&2; exit 1; }
[[ -n "$OVMF_CODE" && -f "$OVMF_CODE" ]] || { echo "OVMF CODE firmware not found" >&2; exit 1; }
[[ -n "$OVMF_VARS_TEMPLATE" && -f "$OVMF_VARS_TEMPLATE" ]] || { echo "OVMF VARS firmware not found" >&2; exit 1; }

TMP=${TMPDIR:-/tmp}/un-orion-openwrt-$$
mkdir -p "$TMP"
cp "$OVMF_VARS_TEMPLATE" "$TMP/orion-vars.fd"

cleanup() {
  set +e
  [[ -n "${ORION_PID:-}" ]] && kill "$ORION_PID" 2>/dev/null
  [[ -n "${OPENWRT_PID:-}" ]] && kill "$OPENWRT_PID" 2>/dev/null
  rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

# OpenWrt has two NICs:
#   eth0/WAN -> QEMU user NAT
#   eth1/LAN -> socket hub exposed to UN_Orion
# The LAN defaults used by UN_Orion's `openwrt` profile are 192.168.1.1/24.
"$QEMU" -name Orion-OpenWrt -m 256M \
  -drive file="$OPENWRT_IMAGE",format=raw,if=virtio \
  -netdev user,id=wan -device virtio-net-pci,netdev=wan \
  -netdev socket,id=lan,listen=:12345 -device rtl8139,netdev=lan,romfile= \
  -nographic >"$TMP/openwrt.log" 2>&1 &
OPENWRT_PID=$!

sleep 2

"$QEMU" -name UN-Orion-OpenWrt-Lab -machine q35 -m 512M \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$TMP/orion-vars.fd" \
  -drive format=raw,file="$ORION_IMG" \
  -netdev socket,id=n0,connect=127.0.0.1:12345 -device rtl8139,netdev=n0,romfile= \
  -serial stdio &
ORION_PID=$!
wait "$ORION_PID"
