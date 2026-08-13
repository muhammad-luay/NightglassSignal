#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/full-campaign-frames-$(date +%Y%m%d-%H%M%S)}"
MAX_START_SECONDS="${MAX_START_SECONDS:-30}"

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
WINDOW_ID=""

cleanup() {
    if [[ -n "${GAME_PID}" ]] && kill -0 "${GAME_PID}" 2>/dev/null; then
        kill "${GAME_PID}" 2>/dev/null || true
    fi
    if [[ -n "${GAME_PID}" ]]; then
        wait "${GAME_PID}" 2>/dev/null || true
    fi
    GAME_PID=""
    WINDOW_ID=""
}
trap cleanup EXIT INT TERM

resolve_game_window() {
    for _ in {1..60}; do
        WINDOW_ID="$(swift -e 'import CoreGraphics; import Foundation; let pid = pid_t(CommandLine.arguments[1])!; let screen = CGDisplayBounds(CGMainDisplayID()); if let list = CGWindowListCopyWindowInfo(.optionAll, kCGNullWindowID) as? [[String: Any]] { var bestID: UInt32 = 0; var bestArea = 0; for item in list { guard let ownerPID = item[kCGWindowOwnerPID as String] as? Int, ownerPID == Int(pid), let bounds = item[kCGWindowBounds as String] as? [String: Any], let width = bounds["Width"] as? Int, let height = bounds["Height"] as? Int, let x = bounds["X"] as? Int, let y = bounds["Y"] as? Int, width > 400, height > 300, CGFloat(x + width) > screen.minX, CGFloat(x) < screen.maxX, CGFloat(y + height) > screen.minY, CGFloat(y) < screen.maxY else { continue }; let area = width * height; if area > bestArea { bestArea = area; if let number = item[kCGWindowNumber as String] as? UInt32 { bestID = number } else if let number = item[kCGWindowNumber as String] as? Int { bestID = UInt32(number) } } }; if bestID > 0 { print(bestID) } }' "${GAME_PID}" 2>/dev/null | tail -1)"
        if [[ -n "${WINDOW_ID}" ]]; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

capture_frame() {
    local stage="$1"
    local destination="$2"
    resolve_game_window || return 1
    for _ in {1..20}; do
        if screencapture -x -o -l"${WINDOW_ID}" "${destination}" >/dev/null 2>&1 && [[ -s "${destination}" ]]; then
            echo "${stage}: captured ${destination}"
            return 0
        fi
        sleep 0.1
    done
    echo "${stage}: capture failed window=${WINDOW_ID}" >&2
    return 1
}

LEVEL_IDS=(
    SableQuay Floodline RelayHouse NightglassCore AshenDocks
    CinderWorks SaltFoundry BlackwaterYard GlassMarket StormVault
    RedTide IronCathedral LanternBasin QuarryNine VantaSpire
    Wreckline EmberStation PaleHarbor MeridianGate LastLight
)

for LevelIndex in "${!LEVEL_IDS[@]}"; do
    LevelId="${LEVEL_IDS[${LevelIndex}]}"
    Prefix="${REPORT_DIR}/${LevelIndex}-${LevelId}"
    STDOUT_PATH="${Prefix}.stdout.log"
    LOG_PATH="${Prefix}.log"
    : >"${STDOUT_PATH}"
    : >"${LOG_PATH}"
    cleanup

    # This is the normal packaged campaign world with the development-only
    # full-campaign driver. It retains the real campaign waves, character
    # movement, player weapon fire, objective holds, extraction, and death
    # presentation; it does not use the combat showcase, fallback enemy, a
    # teleport, or a synthetic wave.
    "${EXECUTABLE}" "/Game/Levels/Glasswake" \
        -game -windowed -ResX=1280 -ResY=720 -NoMutex -NoLiveCoding \
        -NoCrashDialog -nosplash -stdout -FullStdOutLogOutput -NoSound \
        -BM_AutoStartMission -BM_FullCampaignProbe \
        "-BM_CampaignLevel=${LevelIndex}" \
        "-BM_ProfileSlot=FullCampaignFrames_${LevelIndex}" \
        "-abslog=${LOG_PATH}" \
        >"${STDOUT_PATH}" 2>&1 &
    GAME_PID=$!
    osascript -e 'tell application "NightglassSignalDemo" to activate' >/dev/null 2>&1 || true

    insertion=0
    encounter=0
    handoff=0
    extraction=0
    for ((Tenth = 0; Tenth < MAX_START_SECONDS * 10; ++Tenth)); do
        if (( ! insertion )) && rg -q "Black Meridian UI: mission presentation screen=10" "${STDOUT_PATH}"; then
            sleep 0.55
            capture_frame insertion "${Prefix}-insertion.png" || exit 1
            insertion=1
        fi
        if (( ! encounter )) && rg -q "Black Meridian full campaign probe: combat start encounter=checkpoint" "${STDOUT_PATH}"; then
            sleep 0.30
            capture_frame first-encounter "${Prefix}-first-encounter.png" || exit 1
            encounter=1
        fi
        if (( ! handoff )) && rg -q "Black Meridian mission: phase 3 -> 4" "${STDOUT_PATH}"; then
            sleep 0.30
            capture_frame post-clear "${Prefix}-post-clear.png" || exit 1
            handoff=1
        fi
        if (( ! extraction )) && rg -q "Black Meridian mission: phase 7 -> 8" "${STDOUT_PATH}"; then
            sleep 0.30
            capture_frame extraction "${Prefix}-extraction.png" || exit 1
            extraction=1
        fi
        if (( insertion && encounter && handoff && extraction )); then
            break
        fi
        if rg -q "Black Meridian full campaign probe: FAIL" "${STDOUT_PATH}"; then
            echo "${LevelId}: full campaign probe failed during frame capture" >&2
            rg "Black Meridian full campaign probe: FAIL" "${STDOUT_PATH}" | tail -3 >&2
            exit 1
        fi
        if ! kill -0 "${GAME_PID}" 2>/dev/null; then
            echo "${LevelId}: game exited before all campaign frame markers" >&2
            tail -120 "${STDOUT_PATH}" >&2 || true
            exit 1
        fi
        sleep 0.1
    done

    cleanup
    if (( ! insertion || ! encounter || ! handoff || ! extraction )); then
        echo "${LevelId}: timed out waiting for rendered frame markers" >&2
        exit 1
    fi
    echo "${LevelIndex} ${LevelId}: PASS"
done

echo "full campaign frame capture passed: ${#LEVEL_IDS[@]} levels with insertion, first encounter, post-clear, and extraction frames"
