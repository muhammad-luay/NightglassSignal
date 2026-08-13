#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/campaign-level-screenshots-20260811}"
MAX_START_SECONDS="${MAX_START_SECONDS:-30}"

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

EXECUTABLE="${STAGED_APP}/Contents/MacOS/NightglassSignalDemo"
if [[ ! -x "${EXECUTABLE}" ]]; then
    echo "Packaged executable is missing or not executable: ${EXECUTABLE}" >&2
    exit 2
fi

LEVEL_IDS=(
    SableQuay Floodline RelayHouse NightglassCore AshenDocks
    CinderWorks SaltFoundry BlackwaterYard GlassMarket StormVault
    RedTide IronCathedral LanternBasin QuarryNine VantaSpire
    Wreckline EmberStation PaleHarbor MeridianGate LastLight
)

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

resolve_game_window() {
    local pid="$1"
    local window_id=""
    for _ in {1..40}; do
        window_id="$(swift -e 'import CoreGraphics; import Foundation; let pid = pid_t(CommandLine.arguments[1])!; let screen = CGDisplayBounds(CGMainDisplayID()); let options = CGWindowListOption(arrayLiteral: .optionAll); if let list = CGWindowListCopyWindowInfo(options, kCGNullWindowID) as? [[String: Any]] { var bestID: UInt32 = 0; var bestArea = 0; for item in list { guard let ownerPID = item[kCGWindowOwnerPID as String] as? Int, ownerPID == Int(pid), let bounds = item[kCGWindowBounds as String] as? [String: Any], let width = bounds["Width"] as? Int, let height = bounds["Height"] as? Int, let x = bounds["X"] as? Int, let y = bounds["Y"] as? Int, width > 400, height > 300, CGFloat(x + width) > screen.minX, CGFloat(x) < screen.maxX, CGFloat(y + height) > screen.minY, CGFloat(y) < screen.maxY else { continue }; let area = width * height; if area > bestArea { bestArea = area; if let number = item[kCGWindowNumber as String] as? UInt32 { bestID = number } else if let number = item[kCGWindowNumber as String] as? Int { bestID = UInt32(number) } } }; if bestID > 0 { print(bestID) } }' "${pid}" 2>/dev/null | tail -1)"
        if [[ -n "${window_id}" ]]; then
            printf '%s\n' "${window_id}"
            return 0
        fi
        sleep 0.1
    done
    return 1
}

for level_index in "${!LEVEL_IDS[@]}"; do
    level_id="${LEVEL_IDS[${level_index}]}"
    level_label="$(printf '%02d-%s' "$((level_index + 1))" "${level_id}")"
    stdout_path="${REPORT_DIR}/${level_label}.stdout.log"
    log_path="${REPORT_DIR}/${level_label}.log"
    screenshot_path="${REPORT_DIR}/${level_label}.png"

    cleanup
    : >"${stdout_path}"
    : >"${log_path}"

    # This is the normal playable campaign entry path. It intentionally does
    # not use BM_CombatShowcase, BM_MissionProbe, teleports, or synthetic waves.
    "${EXECUTABLE}" /Game/Levels/Glasswake \
        -game -windowed -ResX=1280 -ResY=720 -NoMutex -NoLiveCoding \
        -NoCrashDialog -nosplash -stdout -FullStdOutLogOutput -NoSound \
        -BM_AutoStartMission "-BM_CampaignLevel=${level_index}" \
        "-abslog=${log_path}" \
        >"${stdout_path}" 2>&1 &
    GAME_PID=$!

    # Unreal can restore a saved off-screen window position for the first
    # frame, especially after a monitor/layout change. Bring the packaged
    # app forward and let resolve_game_window wait for an on-screen surface.
    osascript -e 'tell application "NightglassSignalDemo" to activate' >/dev/null 2>&1 || true

    ready=0
    for ((second = 0; second < MAX_START_SECONDS * 10; ++second)); do
        if rg -q "Black Meridian UI: mission presentation screen=10" "${stdout_path}" 2>/dev/null; then
            ready=1
            break
        fi
        if ! kill -0 "${GAME_PID}" 2>/dev/null; then
            echo "${level_label}: game exited before normal mission presentation" >&2
            tail -120 "${stdout_path}" >&2 || true
            exit 1
        fi
        sleep 0.1
    done
    if [[ "${ready}" != "1" ]]; then
        echo "${level_label}: normal mission presentation timed out" >&2
        tail -160 "${stdout_path}" >&2 || true
        exit 1
    fi

    # Let the first gameplay frame settle after the normal HUD hand-off.
    sleep 0.5
    window_id="$(resolve_game_window "${GAME_PID}")" || {
        echo "${level_label}: could not resolve the packaged game window" >&2
        exit 1
    }
    if ! screencapture -x -o -l"${window_id}" "${screenshot_path}" >/dev/null 2>&1 || [[ ! -s "${screenshot_path}" ]]; then
        echo "${level_label}: screenshot capture failed" >&2
        exit 1
    fi

    if ! rg -q "Black Meridian campaign layout: level=${level_id} .* structural_pieces=[1-9]" "${stdout_path}"; then
        echo "${level_label}: screenshot exists but layout evidence is missing" >&2
        exit 1
    fi
    if ! rg -q "Black Meridian environment profile: .* ambience=${level_id} " "${stdout_path}"; then
        echo "${level_label}: screenshot exists but environment evidence is missing" >&2
        exit 1
    fi

    echo "${level_label}: PASS ${screenshot_path}"
    cleanup
done

echo "campaign level screenshot capture passed: ${#LEVEL_IDS[@]} normal campaign levels"
