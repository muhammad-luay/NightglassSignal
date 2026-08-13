#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/staged-map-smoke}"
MAX_SECONDS="${MAX_SECONDS:-60}"

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

EXECUTABLE="${STAGED_APP}/Contents/MacOS/NightglassSignalDemo"
if [[ ! -x "${EXECUTABLE}" ]]; then
    echo "Staged executable is missing or not executable: ${EXECUTABLE}" >&2
    exit 2
fi

mkdir -p "${REPORT_DIR}"
ACTIVE_PID=""

cleanup() {
    if [[ -n "${ACTIVE_PID}" ]] && kill -0 "${ACTIVE_PID}" 2>/dev/null; then
        kill "${ACTIVE_PID}" 2>/dev/null || true
    fi
    if [[ -n "${ACTIVE_PID}" ]]; then
        wait "${ACTIVE_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

verify_map() {
    local MAP_ID="$1"
    local MAP_URL="$2"
    local LOG_PATH="${REPORT_DIR}/${MAP_ID}.log"
    local STDOUT_PATH="${REPORT_DIR}/${MAP_ID}.stdout.log"
    local SECOND

    : > "${LOG_PATH}"
    : > "${STDOUT_PATH}"
    "${EXECUTABLE}" "${MAP_URL}" \
        -NoMutex -nullrhi -NoSound -NoCrashDialog -unattended -nop4 -nosplash \
        -windowed -ResX=1280 -ResY=720 -log \
        "-abslog=${LOG_PATH}" >"${STDOUT_PATH}" 2>&1 &
    ACTIVE_PID=$!

    for ((SECOND = 0; SECOND < MAX_SECONDS; ++SECOND)); do
        if rg -q \
            -e "LogGlobalStatus: LoadMap Load map complete /Game/Levels/${MAP_ID}" \
            -e "Black Meridian authored level shell: map=${MAP_ID}" \
            "${LOG_PATH}" "${STDOUT_PATH}"; then
            echo "staged map passed: ${MAP_ID}"
            kill "${ACTIVE_PID}" 2>/dev/null || true
            wait "${ACTIVE_PID}" 2>/dev/null || true
            ACTIVE_PID=""
            return 0
        fi
        if ! kill -0 "${ACTIVE_PID}" 2>/dev/null; then
            echo "staged map process exited before loading: ${MAP_ID}" >&2
            tail -80 "${LOG_PATH}" >&2 || true
            exit 1
        fi
        sleep 1
    done

    echo "staged map timed out: ${MAP_ID}" >&2
    tail -100 "${LOG_PATH}" >&2 || true
    exit 1
}

verify_map "Glasswake" "/Game/Levels/Glasswake"
verify_map "SignalControl" "/Game/Levels/SignalControl?game=/Script/BlackMeridianRuntime.BMSignalControlGameMode"
echo "staged map smoke passed"
