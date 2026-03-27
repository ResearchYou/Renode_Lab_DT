# Digital Twin Protocol Challenge

Implement and iterate firmware in `firmware/main.c`.

## Goal
- Build a packetized protocol between master and slave in Renode.
- Ensure all packets pass checksum and ordering checks.
- Ensure bus values match protocol definitions.

## Files
- Firmware code: `firmware/main.c`
- Protocol helpers: `firmware/protocol.h`
- Harness output: `output/*.txt`

## Workflow
1. Edit code in the left editor pane.
2. The results panel on the right updates when you refresh it after a test run.
3. Run tests:
   - `docker-compose up spi-twin` (the test runner)

## Optional PDF
Place a PDF statement in this folder (e.g. `problem.pdf`) and open it in a side tab.
