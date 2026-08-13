#!/usr/bin/env bash
set -euo pipefail

REMOTE_HOST="${BM_REMOTE_INPUT_HOST:-pro}"
REMOTE_PORT="${BM_REMOTE_INPUT_PORT:-1986}"

if [[ $# -gt 0 && "$1" != -* ]]; then
    REMOTE_HOST="$1"
    shift
fi
if [[ $# -gt 0 && "$1" != -* ]]; then
    REMOTE_PORT="$1"
    shift
fi

if [[ ! "${REMOTE_PORT}" =~ ^[0-9]+$ || "${REMOTE_PORT}" -lt 1024 || "${REMOTE_PORT}" -gt 65535 ]]; then
    echo "Remote input port must be an integer from 1024 to 65535: ${REMOTE_PORT}" >&2
    exit 2
fi

if ! command -v ssh >/dev/null 2>&1; then
    echo "ssh is required" >&2
    exit 2
fi

# The game listener is deliberately bound to 127.0.0.1 on the remote Mac.
# SSH carries the client stream to that loopback socket; no game port is
# exposed to the LAN or tailnet. A command is one newline-delimited bridge
# request. With no commands, keep an interactive bridge session open.
REMOTE_COMMAND="nc 127.0.0.1 ${REMOTE_PORT}"
if [[ $# -gt 0 ]]; then
    printf '%s\n' "$@" | ssh -T "${REMOTE_HOST}" "${REMOTE_COMMAND}"
else
    exec ssh -T "${REMOTE_HOST}" "${REMOTE_COMMAND}"
fi
