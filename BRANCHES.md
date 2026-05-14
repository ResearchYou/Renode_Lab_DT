# Branch Strategy

## Canonical branch naming

Scenario branches must use:

`<hardware_setup_brief>/<test_scenario>`

Examples in this repository:
- `rp2040/hello-world`
- `stm32-dual/spi-protocol-v1`

## Baseline and scenario policy

- `master`: barebones baseline application and shared tooling.
- Scenario branches: may differ in firmware, hardware setup, and scenario-specific test harness/scripts.
- Shared tooling (IDE/report/UI plumbing) should remain aligned with `master` unless change is truly scenario-specific.

## New Features and Fixes to Toolkit

New features and fixes to the shared toolkit should be developed in branches diverging from `master`, named according to this branch strategy, and merged back to `master` when stable: `master` -> `dev/<brief feature description>` -> PR, merge with `master`.

## Legacy branches

Legacy `feature/*` branches are temporarily retained but considered deprecated:
- `feature/interface_update` (deprecated)
- `feature/spi_stm32` (deprecated)

Do not use `feature/*` for new work.
