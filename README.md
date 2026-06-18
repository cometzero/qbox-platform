# QBox Platform Overlay

This repository is the Apollo/RD-Aspen platform overlay for QBox. It is built
against a separate QBox core source tree and a separate QEMU/libqemu source
tree.

In the Arm Auto Solutions workspace the expected topology is:

```text
tools/qbox/            QBox core: platforms-vp, libqbox/libqemu integration,
                       reusable SystemC and QEMU-backed components
tools/qbox-platform/   Apollo/RD-Aspen overlay: Lua platforms, Zena/RSE
                       models, Apollo-specific wrappers and tests
tools/qemu/            checkout-local QEMU/libqemu source used by QBox
```

Apollo and RD-Aspen platform source should live in this overlay, not in the
QBox core tree.

## Owned Surface

Primary platform entrypoints:

```text
platforms/apollo/apollo-qvp.lua
platforms/apollo/apollo-pc.lua
platforms/apollo/apollo-si-cl1.lua
platforms/fvp-rd-aspen-rse/conf.lua
```

Apollo and RD-Aspen hardware block helpers live under:

```text
platforms/apollo/hw-block/
platforms/fvp-rd-aspen/
platforms/fvp-rd-aspen-rse/
```

Overlay-owned component source includes Zena/RSE hardware models and
Apollo-specific wrappers under:

```text
systemc-components/
qemu-components/cc3xx_native/
tests/components/
```

The Apollo and RD-Aspen direct-boot configurations also share the retained
helper:

```text
platforms/ubuntu/fw/arm64_bootloader.lua
```

Reusable core components should be upstreamed or kept in `tools/qbox`.

## Workspace Build

From the Arm Auto Solutions workspace root, build the Apollo QBox platform
dependencies with:

```bash
./local-build.sh qbox
```

The workspace helper configures this overlay as the CMake source tree and
passes the core and QEMU source paths explicitly:

```text
QBOX_CORE_DIR=tools/qbox
QBOX_PLATFORM_DIR=tools/qbox-platform
QBOX_PLATFORM_BUILD_DIR=build/local-apollo-fvp/work/qbox-platform
QBOX_QEMU_DIR=tools/qemu
```

`QBOX_BUILD_DIR` is accepted only as a compatibility alias for
`QBOX_PLATFORM_BUILD_DIR`.

The default aggregate target is:

```text
apollo_fvp_full_system
```

## Manual Configure

When debugging CMake directly from the workspace root:

```bash
cmake \
  -S tools/qbox-platform \
  -B build/local-apollo-fvp/work/qbox-platform \
  -DCMAKE_BUILD_TYPE=Release \
  -DQBOX_CORE_SOURCE_DIR="${PWD}/tools/qbox" \
  -DQBOX_QEMU_SOURCE_DIR="${PWD}/tools/qemu" \
  -DLIBQEMU_GIT="file://${PWD}/tools/qemu" \
  -DFETCHCONTENT_SOURCE_DIR_LIBQEMU="${PWD}/tools/qemu"

cmake --build build/local-apollo-fvp/work/qbox-platform \
  --target apollo_fvp_full_system \
  --parallel 8
```

For focused iteration, replace `apollo_fvp_full_system` with a component or
test target, for example `mhu320ae-tests`, `mmu720ae`, `remote_cpu`, or
`platforms-vp`.

## Run

After local build artifacts exist, launch the Apollo full-system QBox demo from
the workspace root:

```bash
./run_qbox.sh
```

For bounded headless validation:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --si-mode live-cl0-cl1 \
  --skip-build \
  --timeout 900 \
  --post-login-probe
```

Primary-compute direct boot and SI CL1 isolated smoke paths remain available:

```bash
python3 scripts/run/run_qbox_apollo_fvp_linux.py --timeout 600 --post-login-probe
python3 scripts/run/run_qbox_apollo_fvp_si_cl1.py --timeout 300
```

Runtime evidence is written under `build/qbox-apollo-fvp/`.

## Packaging

To package existing local-build outputs into a QBox-runnable image set:

```bash
./local-build.sh package
./run_qbox.sh --local-build-dir build/local-apollo-fvp/package/qbox/local-build
```

## Related Docs

- Root workspace guide: `README.md`
- Agent/source ownership guide: `AGENTS.md`
- Apollo platform guide: `platforms/apollo/README.md`
- RD-Aspen platform guide: `platforms/fvp-rd-aspen/README.md`
