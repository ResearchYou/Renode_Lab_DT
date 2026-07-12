# GhostTag reference implementation

`reference/firmware/` is a complete drop-in replacement for the participant
firmware seed. It implements canonical SipHash-2-4, domain-separated ephemeral
IDs and MACs, constant-time payload comparison, and the same Zephyr broadcaster
as the starter.

Validate it without modifying the participant tree:

```bash
mkdir -p output
podman run --rm \
  -e TAG_COUNT=6 -e SIMULATION_SECONDS=6 \
  -v "$PWD/reference/firmware:/workspace/firmware:ro,Z" \
  -v "$PWD/output:/workspace/output:Z" \
  renode_dt-digital-twin:nrf52840-swarm-ghosttag-apocalypse
```

The authoritative end marker is:

```text
>>> GHOSTTAG FLEET SURVIVED THE APOCALYPSE <<<
```
