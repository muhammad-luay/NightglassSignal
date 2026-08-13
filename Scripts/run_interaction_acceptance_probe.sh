#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/interaction-acceptance-probe}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
RUN_ID="$(date +%Y%m%d-%H%M%S)"
LOG_PATH="${LOG_PATH:-${REPORT_DIR}/InteractionAcceptanceProbe-${RUN_ID}.log}"
MAX_SECONDS="${MAX_SECONDS:-90}"

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

"${EDITOR}" "${PROJECT_PATH}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_AutoStartMission -BM_InteractionAcceptanceProbe \
    "-abslog=${LOG_PATH}" \
    >"${REPORT_DIR}/InteractionAcceptanceProbe.stdout.log" 2>&1 &
GAME_PID=$!

cleanup() {
    if kill -0 "${GAME_PID}" 2>/dev/null; then
        kill "${GAME_PID}" 2>/dev/null || true
    fi
    wait "${GAME_PID}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian interaction acceptance probe: PASS" "${LOG_PATH}" 2>/dev/null; then
        echo "Black Meridian interaction acceptance probe passed"
        rg "Black Meridian interaction acceptance probe: (cover probe|mounted|destruction|revive|PASS)" "${LOG_PATH}" || true
        exit 0
    fi
    if rg -q "Black Meridian interaction acceptance probe: FAIL" "${LOG_PATH}" 2>/dev/null; then
        echo "Black Meridian interaction acceptance probe failed" >&2
        rg "Black Meridian interaction acceptance probe: (cover probe|mounted|destruction|revive|FAIL)" "${LOG_PATH}" >&2 || true
        exit 1
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        echo "Unreal game exited before interaction acceptance completion" >&2
        tail -120 "${LOG_PATH}" >&2 || true
        exit 1
    fi
    sleep 1
done

echo "Black Meridian interaction acceptance probe timed out after ${MAX_SECONDS}s" >&2
tail -160 "${LOG_PATH}" >&2 || true
exit 1
