#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
MAP_PATH="${MAP_PATH:-/Game/Levels/Glasswake}"
REPORT_ROOT="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/aim-validation-probe}"
MAX_SECONDS="${MAX_SECONDS:-45}"

if [[ "${REPORT_ROOT}" != /* ]]; then
    REPORT_ROOT="${PROJECT_ROOT}/${REPORT_ROOT}"
fi

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

RUN_ID="$(date +%Y%m%d-%H%M%S)"
RUN_DIR="${REPORT_ROOT}/${RUN_ID}"
mkdir -p "${RUN_DIR}"
STDOUT_PATH="${RUN_DIR}/AimValidation.stdout.log"
LOG_PATH="${RUN_DIR}/AimValidation.log"
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

"${EDITOR}" "${PROJECT_PATH}" "${MAP_PATH}" \
    -game -NoMutex -NoLiveCoding -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_CampaignLevel=0 -BM_AutoStartMission -BM_AimValidationProbe -BM_CombatProbe \
    -name=NightglassAimValidationProbe -abslog="${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian aim validation probe: PASS" "${LOG_PATH}" 2>/dev/null; then
        echo "Aim validation probe passed: ${RUN_DIR}"
        rg "Black Meridian aim validation probe:" "${LOG_PATH}"
        exit 0
    fi
    if rg -q "Black Meridian aim validation probe: FAIL" "${LOG_PATH}" 2>/dev/null; then
        rg "Black Meridian aim validation probe:" "${LOG_PATH}" >&2
        exit 1
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        echo "Unreal game exited before aim validation completed" >&2
        rg "Black Meridian aim validation probe:" "${LOG_PATH}" >&2 || true
        tail -80 "${LOG_PATH}" >&2 || true
        exit 1
    fi
    sleep 1
done

echo "Aim validation probe timed out after ${MAX_SECONDS}s" >&2
rg "Black Meridian aim validation probe:" "${LOG_PATH}" >&2 || true
exit 1
