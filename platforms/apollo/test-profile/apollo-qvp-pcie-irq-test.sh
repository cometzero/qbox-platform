#!/bin/sh

set -eu

PATH=/sbin:/usr/sbin:/bin:/usr/bin
export PATH

manifest=/usr/share/apollo-pcie-its/input-manifest.json
mode_file=/etc/apollo-pcie-its-mode
probe=/usr/bin/apollo-pcie-its-guest

[ -r "$manifest" ]
[ -r "$mode_file" ]
[ -x "$probe" ]

mode=$(cat "$mode_file")
cmdline=$(cat /proc/cmdline)
case "$mode" in
    msix)
        case " $cmdline " in
            *" pci=nomsi "*) exit 64 ;;
        esac
        ;;
    intx)
        case " $cmdline " in
            *" pci=nomsi "*) ;;
            *) exit 65 ;;
        esac
        ;;
    *) exit 66 ;;
esac

endpoint=/sys/bus/pci/devices/0000:00:01.0
[ -d "$endpoint" ]
endpoint_real=$(readlink -f "$endpoint")
target=
for net in /sys/class/net/*; do
    case "$(readlink -f "$net/device")" in
        "$endpoint_real"/*) target=${net##*/}; break ;;
    esac
done
[ -n "$target" ]
ip link set "$target" up
timeout 15 udhcpc -n -q -t 3 -T 1 -i "$target"

input_sha256=$(sha256sum "$manifest")
input_sha256=${input_sha256%% *}
exec "$probe" qbox "$mode" "$input_sha256"
