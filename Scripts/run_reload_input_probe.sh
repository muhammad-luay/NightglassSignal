#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/reload-input-probe}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
MAX_SECONDS="${MAX_SECONDS:-30}"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
LOG_PATH="${LOG_PATH:-${REPORT_DIR}/ReloadInputProbe-${RUN_ID}.log}"
STDOUT_PATH="${STDOUT_PATH:-${REPORT_DIR}/ReloadInputProbe-${RUN_ID}.stdout.log}"

case "$(uname -s)" in
    Darwin)
        EDITOR="${UE_EDITOR:-${UE_ROOT}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
        ;;
    Linux)
        EDITOR="${UE_EDITOR:-${UE_ROOT}/Engine/Binaries/Linux/UnrealEditor-Cmd}"
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
PROBE_ARGS=("-BM_ReloadInputProbe")
if [[ "${RELOAD_PROBE_EMPTY:-1}" != "0" ]]; then
    PROBE_ARGS+=("-BM_ReloadInputProbeEmpty")
fi

"${EDITOR}" "${PROJECT_PATH}" "/Game/Levels/Glasswake" \
    "${PROBE_ARGS[@]}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    "-abslog=${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

cleanup() {
    if kill -0 "${GAME_PID}" 2>/dev/null; then
        kill "${GAME_PID}" 2>/dev/null || true
    fi
    wait "${GAME_PID}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian reload input probe: PASS" "${LOG_PATH}" 2>/dev/null; then
        rg "Black Meridian reload input probe:" "${LOG_PATH}"
        exit 0
    fi
    if rg -q "Black Meridian reload input probe: FAIL" "${LOG_PATH}" 2>/dev/null; then
        rg "Black Meridian reload input probe:" "${LOG_PATH}" >&2
        exit 1
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        echo "Reload input probe exited before completion" >&2
        tail -100 "${LOG_PATH}" 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

echo "Reload input probe timed out after ${MAX_SECONDS}s" >&2
rg "Black Meridian reload input probe:" "${LOG_PATH}" >&2 || true
exit 1
