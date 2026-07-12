# Online source ledger

Verified on 2026-07-12. These are the primary sources used for nontrivial tool,
API, board, and orchestration decisions.

## Renode

- [Supported boards](https://renode.readthedocs.io/en/latest/introduction/supported-boards.html): confirms the Nordic nRF52840 DK/platform is supported.
- [Bluetooth Low Energy simulation](https://renode.readthedocs.io/en/latest/tutorials/ble-simulation.html): official two-nRF52840 Zephyr demo, `CreateBLEMedium`, radio connection, and 10 microsecond global quantum.
- [Wireless networking](https://renode.readthedocs.io/en/latest/networking/wireless.html): BLE/802.15.4 media, 3D positions, range and deterministic loss functions.
- [Monitor and script syntax](https://renode.readthedocs.io/en/latest/basic/monitor-syntax.html): machine creation, platform loading, paths, variables, and scripts.
- [Renode v1.16.1 release](https://github.com/renode/renode/releases/tag/v1.16.1): pinned stable emulator release and portable asset.
- [`nrf52840.repl` at v1.16.1](https://github.com/renode/renode/blob/v1.16.1/platforms/cpus/nrf52840.repl): exact CPU, memory, UART, timer, crypto, and radio model used by the scenario.

## Zephyr

- [Zephyr 4.4.1 release](https://github.com/zephyrproject-rtos/zephyr/releases/tag/v4.4.1): pinned RTOS patch release.
- [Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html): Ubuntu dependencies, west workspace, package resolver, SDK install, and build flow.
- [West built-in commands](https://docs.zephyrproject.org/latest/develop/west/built-in.html): local-manifest workspace initialization, project-selective update, and narrow fetch semantics used to keep the runtime image deployable.
- [West manifests](https://docs.zephyrproject.org/latest/develop/west/manifest.html): active-project and module-discovery behavior behind the Nordic-only workspace.
- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html): matching SDK discovery and `ZEPHYR_TOOLCHAIN_VARIANT=zephyr`.
- [nRF52840 DK](https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dk/doc/index.html): current board target `nrf52840dk/nrf52840`.
- [BLE Broadcaster sample](https://docs.zephyrproject.org/latest/samples/bluetooth/broadcaster/README.html): manufacturer-data advertising pattern.
- [BLE Observer sample](https://docs.zephyrproject.org/latest/samples/bluetooth/observer/README.html): scan/observer pattern used by gateways.
- [Bluetooth GAP API](https://docs.zephyrproject.org/latest/doxygen/html/group__bt__gap.html): scan callback, AD parsing, advertising start/update behavior.

## Protocol primitive

- [SipHash paper](https://eprint.iacr.org/2012/351.pdf): SipHash-2-4 construction and security goal for short-input PRF/MAC use.
- [Authors' reference implementation](https://github.com/veorq/SipHash): canonical known-answer vectors and CC0 reference code lineage.

SipHash here protects short simulation packets; it is not a claim that this
hackathon design has completed a production tracker security review.

## Kubernetes

- [Jobs](https://kubernetes.io/docs/concepts/workloads/controllers/job/): Indexed completion mode and `JOB_COMPLETION_INDEX`.
- [Pod topology spread constraints](https://kubernetes.io/docs/concepts/scheduling-eviction/topology-spread-constraints/): `ScheduleAnyway` soft spreading across node hostnames.
- [Resource management](https://kubernetes.io/docs/concepts/configuration/manage-resources-containers/): scheduling by requests and cgroup enforcement of limits.

## Participant environment

- [code-server v4.128.0](https://github.com/coder/code-server/releases/tag/v4.128.0): pinned browser IDE release used by the participant image.
- [Docker Compose v5.3.1](https://github.com/docker/compose/releases/tag/v5.3.1): pinned standalone client used by the IDE runner.
