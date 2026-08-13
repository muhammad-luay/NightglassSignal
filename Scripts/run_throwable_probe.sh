#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/throwable-probe}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
MAX_SECONDS="${MAX_SECONDS:-20}"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
LOG_PATH="${REPORT_DIR}/ThrowableProbe-${RUN_ID}.log"
STDOUT_PATH="${REPORT_DIR}/ThrowableProbe-${RUN_ID}.stdout.log"

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
"${EDITOR}" "${PROJECT_PATH}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_ThrowableProbe \
    "-abslog=${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian throwable probe: PASS" "${LOG_PATH}" 2>/dev/null; then
        wait "${PID}" 2>/dev/null || true
        echo "Black Meridian throwable probe passed"
        rg "Black Meridian throwable probe: (spawned|frag detonated|Veil detonated|PASS)" "${LOG_PATH}"
        exit 0
    fi
    if rg -q "Black Meridian throwable probe: FAIL" "${LOG_PATH}" 2>/dev/null; then
        echo "Black Meridian throwable probe failed" >&2
        tail -120 "${LOG_PATH}" >&2 || true
        wait "${PID}" 2>/dev/null || true
        exit 1
    fi
    if ! kill -0 "${PID}" 2>/dev/null; then
        echo "Throwable probe Unreal process exited before the expected result" >&2
        tail -120 "${LOG_PATH}" >&2 || true
        wait "${PID}" 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

echo "Black Meridian throwable probe timed out after ${MAX_SECONDS}s" >&2
tail -120 "${LOG_PATH}" >&2 || true
kill "${PID}" 2>/dev/null || true
wait "${PID}" 2>/dev/null || true
exit 1
