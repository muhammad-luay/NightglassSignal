#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/full-campaign-probe-$(date +%Y%m%d-%H%M%S)}"
MAX_SECONDS="${MAX_SECONDS:-120}"

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

EXECUTABLE="${STAGED_APP}/Contents/MacOS/NightglassSignalDemo"
if [[ ! -x "${EXECUTABLE}" ]]; then
    echo "Packaged executable is missing or not executable: ${EXECUTABLE}" >&2
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
    GAME_PID=""
}
trap cleanup EXIT INT TERM

LEVEL_IDS=(
    SableQuay Floodline RelayHouse NightglassCore AshenDocks
    CinderWorks SaltFoundry BlackwaterYard GlassMarket StormVault
    RedTide IronCathedral LanternBasin QuarryNine VantaSpire
    Wreckline EmberStation PaleHarbor MeridianGate LastLight
)

for LevelIndex in "${!LEVEL_IDS[@]}"; do
    LevelId="${LEVEL_IDS[${LevelIndex}]}"
    LOG_PATH="${REPORT_DIR}/${LevelIndex}-${LevelId}.log"
    STDOUT_PATH="${REPORT_DIR}/${LevelIndex}-${LevelId}.stdout.log"
    : >"${LOG_PATH}"
    : >"${STDOUT_PATH}"

    cleanup
    "${EXECUTABLE}" "/Game/Levels/Glasswake" \
        -game -windowed -ResX=1280 -ResY=720 -NoMutex -NoLiveCoding \
        -NoCrashDialog -nosplash -stdout -FullStdOutLogOutput -NoSound -nullrhi \
        -BM_AutoStartMission -BM_FullCampaignProbe \
        "-BM_CampaignLevel=${LevelIndex}" \
        "-BM_ProfileSlot=FullCampaignImprovements_${LevelIndex}" \
        "-abslog=${LOG_PATH}" \
        >"${STDOUT_PATH}" 2>&1 &
    GAME_PID=$!

    Passed=0
    for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
        if rg -q "Black Meridian full campaign probe: PASS level=${LevelId} traversal=complete combat=complete objectives=relay\+console extraction=boat final_defense=complete phase=Complete" "${STDOUT_PATH}" "${LOG_PATH}"; then
            echo "${LevelIndex} ${LevelId}: PASS"
            rg "Black Meridian full campaign probe: PASS level=${LevelId}" "${STDOUT_PATH}" "${LOG_PATH}" | tail -1
            Passed=1
            break
        fi
        if rg -q "Black Meridian full campaign probe: FAIL" "${STDOUT_PATH}" "${LOG_PATH}"; then
            echo "${LevelIndex} ${LevelId}: FAIL" >&2
            rg "Black Meridian full campaign probe: FAIL" "${STDOUT_PATH}" "${LOG_PATH}" | tail -3 >&2
            exit 1
        fi
        if ! kill -0 "${GAME_PID}" 2>/dev/null; then
            echo "${LevelIndex} ${LevelId}: game exited before PASS/FAIL" >&2
            tail -100 "${STDOUT_PATH}" >&2 || true
            exit 1
        fi
        sleep 1
    done

    cleanup
    if [[ "${Passed}" != "1" ]]; then
        echo "${LevelIndex} ${LevelId}: timed out after ${MAX_SECONDS}s" >&2
        tail -160 "${STDOUT_PATH}" >&2 || true
        exit 1
    fi
done

echo "full campaign probe passed: ${#LEVEL_IDS[@]} levels completed through real traversal, combat, objectives, extraction, and final defense"
