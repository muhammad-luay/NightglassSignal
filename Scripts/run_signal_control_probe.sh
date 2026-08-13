#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
MAP_URL="${MAP_URL:-/Game/Levels/SignalControl?game=/Script/BlackMeridianRuntime.BMSignalControlGameMode?listen}"
CLIENT_MAP_URL="${CLIENT_MAP_URL:-/Game/Levels/SignalControl}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/signal-control-probe}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
if [[ -n "${LOG_PATH:-}" && "${LOG_PATH}" != /* ]]; then
    LOG_PATH="${PROJECT_ROOT}/${LOG_PATH}"
fi
PORT="${PORT:-7787}"
STARTUP_SECONDS="${STARTUP_SECONDS:-90}"
MAX_SECONDS="${MAX_SECONDS:-45}"
NET_EMULATION_ARGS=()
if [[ -n "${PKT_LAG:-}" ]]; then
    NET_EMULATION_ARGS+=("-PktLag=${PKT_LAG}")
fi
if [[ -n "${PKT_LOSS:-}" ]]; then
    NET_EMULATION_ARGS+=("-PktLoss=${PKT_LOSS}")
fi
RUN_ID="$(date +%Y%m%d-%H%M%S)"
HOST_LOG="${LOG_PATH:-${REPORT_DIR}/SignalHost-${RUN_ID}.log}"
CLIENT_A_LOG="${REPORT_DIR}/SignalClientA-${RUN_ID}.log"
CLIENT_B_LOG="${REPORT_DIR}/SignalClientB-${RUN_ID}.log"
HOST_STDOUT="${REPORT_DIR}/SignalHost-${RUN_ID}.stdout.log"
CLIENT_A_STDOUT="${REPORT_DIR}/SignalClientA-${RUN_ID}.stdout.log"
CLIENT_B_STDOUT="${REPORT_DIR}/SignalClientB-${RUN_ID}.stdout.log"
CLIENT_CONNECT_URL="127.0.0.1:${PORT}${CLIENT_MAP_URL}"

case "$(uname -s)" in
    Darwin)
        EDITOR="${UE_EDITOR:-${UE_ROOT}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
        ;;
    Linux)
        EDITOR="${UE_EDITOR:-${UE_ROOT}/Engine/Binaries/Linux/UnrealEditor}"
        ;;
    *)
        echo "Unsupported host OS for this script: $(uname -s)" >&2
        exit 2
        ;;
esac

if [[ ! -x "${EDITOR}" ]]; then
    echo "Unreal editor not found or not executable: ${EDITOR}" >&2
    echo "Set UE_ROOT or UE_EDITOR to a provisioned Unreal installation." >&2
    exit 2
fi

if command -v lsof >/dev/null 2>&1; then
    LISTENERS="$(lsof -nP -iTCP:"${PORT}" -sTCP:LISTEN 2>/dev/null || true)"
    if [[ -n "${LISTENERS}" ]]; then
        echo "Signal Control probe port ${PORT} is already in use; refusing to stop the existing listener:" >&2
        echo "${LISTENERS}" >&2
        exit 1
    fi
fi

mkdir -p "${REPORT_DIR}"

HOST_PID=""
CLIENT_A_PID=""
CLIENT_B_PID=""

cleanup() {
    for PID in "${CLIENT_B_PID}" "${CLIENT_A_PID}" "${HOST_PID}"; do
        if [[ -n "${PID}" ]] && kill -0 "${PID}" 2>/dev/null; then
            kill "${PID}" 2>/dev/null || true
        fi
    done
    for PID in "${CLIENT_B_PID}" "${CLIENT_A_PID}" "${HOST_PID}"; do
        if [[ -n "${PID}" ]]; then
            wait "${PID}" 2>/dev/null || true
        fi
    done
}
trap cleanup EXIT INT TERM

"${EDITOR}" "${PROJECT_PATH}" \
    "${MAP_URL}" \
    "${NET_EMULATION_ARGS[@]+${NET_EMULATION_ARGS[@]}}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_AutoStartSignalMatch -BM_SignalControlProbe -port="${PORT}" \
    "-abslog=${HOST_LOG}" \
    >"${HOST_STDOUT}" 2>&1 &
HOST_PID=$!

for ((Second = 0; Second < STARTUP_SECONDS; ++Second)); do
    if rg -q "Black Meridian signal: authoritative countdown started" "${HOST_LOG}" 2>/dev/null; then
        break
    fi
    if ! kill -0 "${HOST_PID}" 2>/dev/null; then
        echo "Signal Control host exited before opening the match" >&2
        tail -100 "${HOST_LOG}" 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

if ! rg -q "Black Meridian signal: authoritative countdown started" "${HOST_LOG}" 2>/dev/null; then
    echo "Signal Control host did not start its authoritative countdown within ${STARTUP_SECONDS}s" >&2
    tail -100 "${HOST_LOG}" 2>/dev/null || true
    exit 1
fi

wait_for_client_handshake() {
    local CLIENT_PID="$1"
    local CLIENT_LOG="$2"
    local CLIENT_LABEL="$3"
    for ((Second = 0; Second < STARTUP_SECONDS; ++Second)); do
        if rg -q "LogNet: Welcomed by server" "${CLIENT_LOG}" 2>/dev/null; then
            return 0
        fi
        if ! kill -0 "${CLIENT_PID}" 2>/dev/null; then
            echo "Signal Control ${CLIENT_LABEL} exited before completing its network handshake" >&2
            tail -80 "${CLIENT_LOG}" 2>/dev/null || true
            return 1
        fi
        sleep 1
    done
    echo "Signal Control ${CLIENT_LABEL} did not complete its network handshake within ${STARTUP_SECONDS}s" >&2
    tail -80 "${CLIENT_LOG}" 2>/dev/null || true
    return 1
}

"${EDITOR}" "${PROJECT_PATH}" "${CLIENT_CONNECT_URL}" \
    "${NET_EMULATION_ARGS[@]+${NET_EMULATION_ARGS[@]}}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_SignalControlProbe -name=SignalProbeClientA \
    "-abslog=${CLIENT_A_LOG}" \
    >"${CLIENT_A_STDOUT}" 2>&1 &
CLIENT_A_PID=$!

# Serialize Mac client startup so separate editor processes cannot race the
# shared UnrealBuildTool trace file while loading the project module.
wait_for_client_handshake "${CLIENT_A_PID}" "${CLIENT_A_LOG}" "client A"

"${EDITOR}" "${PROJECT_PATH}" "${CLIENT_CONNECT_URL}" \
    "${NET_EMULATION_ARGS[@]+${NET_EMULATION_ARGS[@]}}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_SignalControlProbe -name=SignalProbeClientB \
    "-abslog=${CLIENT_B_LOG}" \
    >"${CLIENT_B_STDOUT}" 2>&1 &
CLIENT_B_PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if ! kill -0 "${HOST_PID}" 2>/dev/null; then
        echo "Signal Control host exited before probe completion" >&2
        tail -120 "${HOST_LOG}" 2>/dev/null || true
        exit 1
    fi

    LOGIN_COUNT="$(rg -c "Black Meridian signal: player login team=" "${HOST_LOG}" 2>/dev/null || true)"
    if [[ "${LOGIN_COUNT:-0}" -ge 2 ]] \
        && rg -q "Black Meridian signal: player login team=1" "${HOST_LOG}" \
        && rg -q "Black Meridian signal: player login team=2" "${HOST_LOG}" \
        && rg -q "Black Meridian signal probe: lobby ready authority=no" "${CLIENT_A_LOG}" 2>/dev/null \
        && rg -q "Black Meridian signal probe: lobby ready authority=no" "${CLIENT_B_LOG}" 2>/dev/null \
        && rg -q "Black Meridian signal: match live" "${HOST_LOG}" \
        && rg -q "Black Meridian signal: active node rotated to 1" "${HOST_LOG}" \
        && rg -q "Black Meridian signal probe: server scoring verified" "${HOST_LOG}"; then
        echo "Signal Control host/client probe passed"
        rg "Black Meridian signal( probe)?:" "${HOST_LOG}"
        echo "-- client A --"
        rg "Black Meridian signal probe:" "${CLIENT_A_LOG}"
        echo "-- client B --"
        rg "Black Meridian signal probe:" "${CLIENT_B_LOG}"
        exit 0
    fi

    if ! kill -0 "${CLIENT_A_PID}" 2>/dev/null || ! kill -0 "${CLIENT_B_PID}" 2>/dev/null; then
        echo "A Signal Control client exited before probe completion" >&2
        tail -80 "${CLIENT_A_LOG}" 2>/dev/null || true
        tail -80 "${CLIENT_B_LOG}" 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

echo "Signal Control host/client probe timed out after ${MAX_SECONDS}s" >&2
rg "Black Meridian signal( probe)?:" "${HOST_LOG}" >&2 || true
rg "Black Meridian signal probe:" "${CLIENT_A_LOG}" >&2 || true
rg "Black Meridian signal probe:" "${CLIENT_B_LOG}" >&2 || true
exit 1
