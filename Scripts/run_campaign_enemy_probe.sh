#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/campaign-enemy-probe}"
MAX_SECONDS="${MAX_SECONDS:-45}"

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
    EVIDENCE_PATHS=("${LOG_PATH}" "${STDOUT_PATH}")

    "${EXECUTABLE}" "/Game/Levels/Glasswake" \
        -game -windowed -ResX=1280 -ResY=720 -NoMutex -NoLiveCoding \
        -NoCrashDialog -nosplash -stdout -FullStdOutLogOutput -NoSound -nullrhi \
        -BM_AutoStartMission -BM_MissionProbe -BM_CampaignEnemyProbe \
        -BM_MissionTimeScale=10 \
        "-BM_CampaignLevel=${LevelIndex}" \
        "-BM_ProfileSlot=CampaignEnemyProbe_${LevelIndex}" \
        "-abslog=${LOG_PATH}" \
        >"${STDOUT_PATH}" 2>&1 &
    GAME_PID=$!

    Passed=0
    for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
        if rg -q \
            "Black Meridian campaign layout: level=${LevelId} .* structural_pieces=[1-9]" \
            "${EVIDENCE_PATHS[@]}" \
            && rg -q \
            "Black Meridian campaign: physical prop set level=${LevelId} .* collision_cover=yes" \
            "${EVIDENCE_PATHS[@]}" \
            && rg -q \
            "Black Meridian campaign encounter_layout=.* level=${LevelId} .* wave_count=[1-9]" \
            "${EVIDENCE_PATHS[@]}" \
            && rg -q \
            "Black Meridian campaign: spawned enemy index=0" \
            "${EVIDENCE_PATHS[@]}"; then
            LayoutLine="$(rg "Black Meridian campaign layout: level=${LevelId}" "${EVIDENCE_PATHS[@]}" | tail -1)"
            EncounterLine="$(rg "Black Meridian campaign encounter_layout=.* level=${LevelId}" "${EVIDENCE_PATHS[@]}" | tail -1)"
            EnemyLine="$(rg "Black Meridian campaign: spawned enemy index=0" "${EVIDENCE_PATHS[@]}" | tail -1)"
            echo "${LevelIndex} ${LevelId}: PASS"
            echo "  ${LayoutLine}"
            echo "  ${EncounterLine}"
            echo "  ${EnemyLine}"
            Passed=1
            break
        fi
        if ! kill -0 "${GAME_PID}" 2>/dev/null; then
            echo "${LevelId}: game exited before enemy/layout evidence" >&2
            tail -140 "${LOG_PATH}" "${STDOUT_PATH}" >&2 || true
            exit 1
        fi
        sleep 1
    done

    cleanup
    GAME_PID=""
    if [[ "${Passed}" != "1" ]]; then
        echo "${LevelId}: timed out after ${MAX_SECONDS}s" >&2
        tail -180 "${LOG_PATH}" "${STDOUT_PATH}" >&2 || true
        exit 1
    fi
done

echo "campaign enemy probe passed: ${#LEVEL_IDS[@]} levels loaded, retained collision layouts, and spawned real campaign enemies"
