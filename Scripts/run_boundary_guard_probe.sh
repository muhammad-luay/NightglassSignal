#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/boundary-guard-edge-check}"
MAP_PATH="${MAP_PATH:-/Game/Levels/Glasswake}"
MAX_SECONDS="${MAX_SECONDS:-30}"

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

EXECUTABLE="${STAGED_APP}/Contents/MacOS/NightglassSignalDemo"
if [[ ! -x "${EXECUTABLE}" ]]; then
    echo "Packaged executable is missing or not executable: ${EXECUTABLE}" >&2
    exit 2
fi

mkdir -p "${REPORT_DIR}"
STDOUT_PATH="${REPORT_DIR}/BoundaryGuardProbe.stdout.log"
LOG_PATH="${REPORT_DIR}/BoundaryGuardProbe.log"
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

"${EXECUTABLE}" "${MAP_PATH}" \
    -BM_CampaignLevel=1 -BM_AutoStartMission -BM_BoundaryGuardProbe \
    -game -windowed -ResX=1280 -ResY=720 -NoMutex -NoLiveCoding \
    -NoCrashDialog -nosplash -stdout -FullStdOutLogOutput -NoSound \
    -abslog="${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian boundary guard probe: PASS" "${STDOUT_PATH}" 2>/dev/null; then
        rg "Black Meridian boundary guard probe:" "${STDOUT_PATH}"
        exit 0
    fi
    if rg -q "Black Meridian boundary guard probe: FAIL" "${STDOUT_PATH}" 2>/dev/null; then
        rg "Black Meridian boundary guard probe:" "${STDOUT_PATH}" >&2
        exit 1
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        echo "Boundary guard probe exited before reporting PASS/FAIL" >&2
        rg "Black Meridian boundary guard probe:" "${STDOUT_PATH}" >&2 || true
        exit 1
    fi
    sleep 1
done

echo "Boundary guard probe timed out after ${MAX_SECONDS}s" >&2
rg "Black Meridian boundary guard probe:" "${STDOUT_PATH}" >&2 || true
exit 1
