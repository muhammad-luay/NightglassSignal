#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_ROOT="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/packaged-combat-showcase}"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
REPORT_DIR="${REPORT_ROOT}/${RUN_ID}"
MAX_SECONDS="${MAX_SECONDS:-30}"
AUTO_FIRE="${AUTO_FIRE:-1}"
VISUAL_ONLY="${VISUAL_ONLY:-0}"
CLOSE_FACING="${CLOSE_FACING:-0}"
AXIS_PROBE="${AXIS_PROBE:-0}"
CAMPAIGN_LEVEL="${CAMPAIGN_LEVEL:-0}"
NO_SOUND="${NO_SOUND:-1}"
FORCE_FALLBACK="${FORCE_FALLBACK:-0}"

if [[ ! "${CAMPAIGN_LEVEL}" =~ ^[0-9]+$ ]] || (( CAMPAIGN_LEVEL < 0 || CAMPAIGN_LEVEL > 19 )); then
    echo "CAMPAIGN_LEVEL must be an integer from 0 to 19" >&2
    exit 2
fi

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

EXECUTABLE="${STAGED_APP}/Contents/MacOS/NightglassSignalDemo"
if [[ ! -x "${EXECUTABLE}" ]]; then
    echo "Packaged executable is missing or not executable: ${EXECUTABLE}" >&2
    exit 2
fi

mkdir -p "${REPORT_DIR}"
SCREENSHOT_EARLY="${REPORT_DIR}/combat-showcase-early.png"
SCREENSHOT_MID="${REPORT_DIR}/combat-showcase-mid.png"
SCREENSHOT_LATE="${REPORT_DIR}/combat-showcase-late.png"
STDOUT_PATH="${REPORT_DIR}/CombatShowcase.stdout.log"
LOG_PATH="${REPORT_DIR}/CombatShowcase.log"
GAME_PID=""
CAPTURED_EARLY=0
CAPTURED_MID=0
CAPTURED_LATE=0

cleanup() {
    if [[ -n "${GAME_PID}" ]] && kill -0 "${GAME_PID}" 2>/dev/null; then
        kill "${GAME_PID}" 2>/dev/null || true
    fi
    if [[ -n "${GAME_PID}" ]]; then
        wait "${GAME_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

capture_game_window() {
    local stage="$1"
    local destination="$2"
    local window_id=""

    for _ in {1..20}; do
        window_id="$(swift -e 'import CoreGraphics; import Foundation; let pid = pid_t(CommandLine.arguments[1])!; let options = CGWindowListOption(arrayLiteral: .optionAll); if let list = CGWindowListCopyWindowInfo(options, kCGNullWindowID) as? [[String: Any]] { var bestID: UInt32 = 0; var bestArea = 0; for item in list { guard let ownerPID = item[kCGWindowOwnerPID as String] as? Int, ownerPID == Int(pid), let bounds = item[kCGWindowBounds as String] as? [String: Any], let width = bounds["Width"] as? Int, let height = bounds["Height"] as? Int, width > 400, height > 300 else { continue }; let area = width * height; if area > bestArea { bestArea = area; if let number = item[kCGWindowNumber as String] as? UInt32 { bestID = number } else if let number = item[kCGWindowNumber as String] as? Int { bestID = UInt32(number) } } }; if bestID > 0 { print(bestID) } }' "${GAME_PID}" 2>/dev/null | tail -1)"
        if [[ -n "${window_id}" ]] && screencapture -x -o -l"${window_id}" "${destination}" >/dev/null 2>&1 && [[ -s "${destination}" ]]; then
            echo "external window capture passed stage=${stage} path=${destination} window=${window_id}"
            return 0
        fi
        sleep 0.1
    done

    echo "external window capture failed stage=${stage} path=${destination}" >&2
    return 1
}

SHOWCASE_ARGS=("-BM_CombatShowcase" "-BM_AutoStartMission")
if [[ "${AUTO_FIRE}" != "0" ]]; then
    SHOWCASE_ARGS+=("-BM_CombatShowcaseAutoFire")
fi
if [[ "${VISUAL_ONLY}" != "0" ]]; then
    # Keep the passive behavior opt-in and unambiguous. BM_CombatShowcase is
    # also a useful playable combat launch, so the showcase flag alone must
    # never disable player or enemy fire.
    SHOWCASE_ARGS+=("-BM_PassiveCombatCapture")
fi
if [[ "${CLOSE_FACING}" != "0" ]]; then
    SHOWCASE_ARGS+=("-BM_CombatCloseFacingProbe")
fi
if [[ "${AXIS_PROBE}" != "0" ]]; then
    SHOWCASE_ARGS+=("-BM_CombatAxisProbe")
fi
if [[ "${FORCE_FALLBACK}" != "0" ]]; then
    SHOWCASE_ARGS+=("-BM_ForceFallbackEnemyPresentation")
fi

SOUND_ARG=""
if [[ "${NO_SOUND}" != "0" ]]; then
    # Keep this as a scalar because the system Bash used by macOS errors when
    # expanding an empty array under `set -u`.
    SOUND_ARG="-NoSound"
fi

"${EXECUTABLE}" "/Game/Levels/Glasswake" \
    "-BM_CampaignLevel=${CAMPAIGN_LEVEL}" \
    "${SHOWCASE_ARGS[@]}" \
    -game -NoMutex -nop4 -nosplash -NoCrashDialog ${SOUND_ARG} \
    -windowed -ResX=1280 -ResY=720 -log -stdout -FullStdOutLogOutput \
    "-abslog=${LOG_PATH}" >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

for ((Tenth = 0; Tenth < MAX_SECONDS * 10; ++Tenth)); do
    if (( ! CAPTURED_EARLY )) && rg -q "Black Meridian showcase: spawned rendered combat wave" "${STDOUT_PATH}" 2>/dev/null; then
        sleep 0.35
        capture_game_window early "${SCREENSHOT_EARLY}" || true
        CAPTURED_EARLY=1
    fi
    if (( CAPTURED_EARLY && ! CAPTURED_MID && Tenth >= 28 )); then
        capture_game_window mid "${SCREENSHOT_MID}" || true
        CAPTURED_MID=1
    fi
    if (( CAPTURED_MID && ! CAPTURED_LATE && Tenth >= 52 )); then
        capture_game_window late "${SCREENSHOT_LATE}" || true
        CAPTURED_LATE=1
    fi
    if (( CAPTURED_EARLY && CAPTURED_MID && CAPTURED_LATE )); then
        break
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        echo "Packaged combat showcase exited before all captures" >&2
        break
    fi
    sleep 0.1
done

if [[ ! -s "${SCREENSHOT_EARLY}" || ! -s "${SCREENSHOT_MID}" || ! -s "${SCREENSHOT_LATE}" ]]; then
    echo "Packaged combat showcase capture failed" >&2
    rg "Black Meridian (showcase: spawned rendered combat wave|showcase: controlled shot|runtime: greybox ready|authored shell:)" "${STDOUT_PATH}" >&2 || true
    exit 1
fi

rg "Black Meridian (showcase: spawned rendered combat wave|showcase: controlled shot|runtime: greybox ready|authored shell:)" "${STDOUT_PATH}" || true
echo "packaged combat showcase capture passed: ${REPORT_DIR}"
