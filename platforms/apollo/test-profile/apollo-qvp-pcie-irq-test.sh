#!/bin/sh

PATH=/sbin:/usr/sbin:/bin:/usr/bin
export PATH

cmdline="$(cat /proc/cmdline 2>/dev/null)"
mode=msix
case " ${cmdline} " in
    *" pci=nomsi "*) mode=intx ;;
esac

echo "__QBOX_PCIE_IRQ_TEST_BEGIN__:${mode}"
echo "__QBOX_PCIE_BDF__:0000:00:01.0"
echo "__QBOX_PCIE_CMDLINE__:${cmdline}"

lspci -nn -s 00:01.0 2>/dev/null
echo "__QBOX_PCIE_LSPCI_RC__:$?"
lspci -vv -s 00:01.0 2>/dev/null |
    sed -n '/MSI-X:/s/^/__QBOX_PCIE_MSIX__:/p'

iface=
for _ in 1 2 3 4 5 6 7 8 9 10; do
    for path in /sys/class/net/*; do
        [ -r "${path}/address" ] || continue
        address="$(cat "${path}/address" 2>/dev/null)"
        if [ "${address}" = "52:54:00:12:34:56" ]; then
            iface="${path##*/}"
            break
        fi
    done
    [ -z "${iface}" ] || break
    sleep 1
done

echo "__QBOX_PCIE_IFACE__:${iface}"
if [ -n "${iface}" ]; then
    ip link set "${iface}" up
    echo "__QBOX_PCIE_LINK_UP_RC__:$?"
fi

awk '/virtio/ { irq=$1; gsub(/:/, "", irq); print irq }' /proc/interrupts |
while read -r irq; do
    case "${irq}" in
        ""|*[!0-9]*) continue ;;
    esac
    echo 1 > "/proc/irq/${irq}/smp_affinity" 2>/dev/null || true
done

echo "__QBOX_PCIE_IRQ_BEFORE__"
cat /proc/interrupts
echo "__QBOX_PCIE_IRQ_BEFORE_END__"

if [ -n "${iface}" ]; then
    udhcpc -n -q -t 3 -T 1 -i "${iface}"
    echo "__QBOX_PCIE_UDHCPC_RC__:$?"
    ping -c 2 -W 2 10.0.2.2
    echo "__QBOX_PCIE_PING_RC__:$?"
fi

echo "__QBOX_PCIE_IRQ_AFTER__"
cat /proc/interrupts
echo "__QBOX_PCIE_IRQ_AFTER_END__"
echo "__QBOX_PCIE_IRQ_TEST_DONE__:${mode}"
