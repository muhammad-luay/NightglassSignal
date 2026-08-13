#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
MAP_URL="${MAP_URL:-/Game/Levels/Glasswake}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/combat-feel-probe}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
RUN_ID="$(date +%Y%m%d-%H%M%S)"
LOG_PATH="${LOG_PATH:-${REPORT_DIR}/CombatFeelProbe-${RUN_ID}.log}"
MAX_SECONDS="${MAX_SECONDS:-60}"

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
STDOUT_PATH="${REPORT_DIR}/CombatFeelProbe-${RUN_ID}.stdout.log"
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

"${EDITOR}" "${PROJECT_PATH}" "${MAP_URL}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_AutoStartMission -BM_CombatFeelProbe -BM_CombatProbe \
    -name=NightglassCombatFeelProbe -abslog="${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian combat feel probe: PASS" "${LOG_PATH}" 2>/dev/null; then
        echo "Combat feel probe passed"
        rg "Black Meridian combat feel probe:" "${LOG_PATH}"
        exit 0
    fi
    if rg -q "Black Meridian combat feel probe: FAIL" "${LOG_PATH}" 2>/dev/null; then
        rg "Black Meridian combat feel probe:" "${LOG_PATH}" >&2
        exit 1
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        echo "Unreal game exited before combat feel probe completion" >&2
        rg "Black Meridian combat feel probe:" "${LOG_PATH}" >&2 || true
        tail -80 "${LOG_PATH}" >&2 || true
        exit 1
    fi
    sleep 1
done

echo "Combat feel probe timed out after ${MAX_SECONDS}s" >&2
rg "Black Meridian combat feel probe:" "${LOG_PATH}" >&2 || true
exit 1
