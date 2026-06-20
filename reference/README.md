# Reference Implementation

This directory contains the completed firmware implementation for the RP2040
sensor filtering lab.

The student TODO variant remains in:

```text
firmware/main.c
```

The reference implementation is:

```text
reference/firmware/main.c
```

It is a drop-in replacement for the student starter. It intentionally reuses the
same project configuration and `firmware/model_weights.h` so there is only one
set of build settings and one set of model constants.

## Validate the Reference

From the repository root:

```bash
sg docker -c 'DOCKER_HOST=unix:///var/run/docker.sock docker run --rm --entrypoint /bin/bash \
  -v "$PWD":/host:ro renode_dt-digital-twin:latest \
  -lc "cp /host/reference/firmware/main.c /workspace/firmware/main.c && /workspace/scripts/entrypoint.sh"'
```

If your current shell already has Docker group membership, omit `sg docker -c`.
Keep the `DOCKER_HOST=unix:///var/run/docker.sock` prefix if your environment
points Docker at a stale rootless socket.

Expected result:

```text
SUMMARY threshold_keep=8 threshold_drop=2 model_keep=6 model_drop=4 disagreements=2
>>> ALL CHECKS PASSED - sensor filters behave as expected <<<
```
