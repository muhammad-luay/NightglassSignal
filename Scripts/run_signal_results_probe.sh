#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
MAP_URL="${MAP_URL:-/Game/Levels/SignalControl?game=/Script/BlackMeridianRuntime.BMSignalControlGameMode}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/signal-results-probe}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
MAX_SECONDS="${MAX_SECONDS:-45}"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
LOG_PATH="${REPORT_DIR}/SignalResults-${RUN_ID}.log"
STDOUT_PATH="${REPORT_DIR}/SignalResults-${RUN_ID}.stdout.log"

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
    exit 2
fi

mkdir -p "${REPORT_DIR}"
GAME_PID=""
cleanup() {
    if [[ -n "${GAME_PID}" ]] && kill -0 "${GAME_PID}" 2>/dev/null; then
        kill "${GAME_PID}" 2>/dev/null || true
    fi
    if [[ -n "${GAME_PID}" ]]; then
        wait "${GAME_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

"${EDITOR}" "${PROJECT_PATH}" \
    "${MAP_URL}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_AutoStartSignalMatch -BM_SignalControlResultsProbe \
    "-abslog=${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian UI: results screen signal_control=yes" "${LOG_PATH}" 2>/dev/null \
        && rg -q "Black Meridian signal: match finished winner=" "${LOG_PATH}" 2>/dev/null; then
        echo "Signal Control results probe passed"
        rg "Black Meridian (signal: match finished|UI: results screen)" "${LOG_PATH}"
        exit 0
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        echo "Signal Control results probe exited before the results screen" >&2
        tail -120 "${LOG_PATH}" >&2 || true
        exit 1
    fi
    sleep 1
done

echo "Signal Control results probe timed out after ${MAX_SECONDS}s" >&2
rg "Black Meridian (signal:|UI: results screen)" "${LOG_PATH}" >&2 || true
exit 1
