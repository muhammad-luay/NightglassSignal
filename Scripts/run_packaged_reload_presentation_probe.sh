#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_ROOT="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/packaged-reload-presentation-probe}"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
REPORT_DIR="${REPORT_ROOT}/${RUN_ID}"
MAX_SECONDS="${MAX_SECONDS:-60}"

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

EXECUTABLE="${STAGED_APP}/Contents/MacOS/NightglassSignalDemo"
if [[ ! -x "${EXECUTABLE}" ]]; then
    echo "Packaged executable is missing or not executable: ${EXECUTABLE}" >&2
    exit 2
fi

mkdir -p "${REPORT_DIR}"
START_SCREENSHOT="${REPORT_DIR}/reload-presentation-start.png"
MID_SCREENSHOT="${REPORT_DIR}/reload-presentation-mid.png"
COMPLETE_SCREENSHOT="${REPORT_DIR}/reload-presentation-complete.png"
ENGINE_START_SCREENSHOT="${REPORT_DIR}/engine-reload-presentation-start.png"
ENGINE_MID_SCREENSHOT="${REPORT_DIR}/engine-reload-presentation-mid.png"
ENGINE_COMPLETE_SCREENSHOT="${REPORT_DIR}/engine-reload-presentation-complete.png"
STDOUT_PATH="${REPORT_DIR}/ReloadPresentation.stdout.log"
LOG_PATH="${REPORT_DIR}/ReloadPresentation.log"
GAME_PID=""
CAPTURED_START=0
CAPTURED_MID=0
CAPTURED_COMPLETE=0
WINDOW_ID=""

cleanup() {
    if [[ -n "${GAME_PID}" ]] && kill -0 "${GAME_PID}" 2>/dev/null; then
        kill "${GAME_PID}" 2>/dev/null || true
    fi
    if [[ -n "${GAME_PID}" ]]; then
        wait "${GAME_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

resolve_game_window() {
    if [[ -n "${WINDOW_ID}" ]]; then
        return 0
    fi

    # Packaged Mac Metal does not expose a readable viewport texture to
    # FViewport::ReadPixels, and FScreenshotRequest can therefore log a
    # request without producing a file. Capture the actual game window from
    # the host instead. The largest window owned by this PID is the game
    # surface; the smaller title-bar/menu windows are ignored.
    #
    # Resolve the window only once. Compiling the Swift CGWindowList probe on
    # every stage takes longer than the reload presentation itself, so a
    # per-stage lookup can capture the already-completed READY state instead
    # of the requested RELOADING state.
    for _ in {1..20}; do
        WINDOW_ID="$(swift -e 'import CoreGraphics; import Foundation; let pid = pid_t(CommandLine.arguments[1])!; let options = CGWindowListOption(arrayLiteral: .optionAll); if let list = CGWindowListCopyWindowInfo(options, kCGNullWindowID) as? [[String: Any]] { var bestID: UInt32 = 0; var bestArea = 0; for item in list { guard let ownerPID = item[kCGWindowOwnerPID as String] as? Int, ownerPID == Int(pid), let bounds = item[kCGWindowBounds as String] as? [String: Any], let width = bounds["Width"] as? Int, let height = bounds["Height"] as? Int, width > 400, height > 300 else { continue }; let area = width * height; if area > bestArea { bestArea = area; if let number = item[kCGWindowNumber as String] as? UInt32 { bestID = number } else if let number = item[kCGWindowNumber as String] as? Int { bestID = UInt32(number) } } }; if bestID > 0 { print(bestID) } }' "${GAME_PID}" 2>/dev/null | tail -1)"
        if [[ -n "${WINDOW_ID}" ]]; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

capture_game_window() {
    local stage="$1"
    local destination="$2"

    resolve_game_window || true

    for _ in {1..20}; do
        if [[ -n "${WINDOW_ID}" ]] && screencapture -x -o -l"${WINDOW_ID}" "${destination}" >/dev/null 2>&1 && [[ -s "${destination}" ]]; then
            echo "external window capture passed stage=${stage} path=${destination} window=${WINDOW_ID}"
            return 0
        fi
        sleep 0.1
    done

    echo "external window capture failed stage=${stage} path=${destination}" >&2
    return 1
}

# Keep stdout redirected to the report without asking Unreal to create its
# separate log window. The host capture helper selects the largest window
# owned by this PID; with -log enabled that can be the text log surface
# instead of the actual game viewport.
"${EXECUTABLE}" "/Game/Levels/Glasswake" \
    -BM_AutoStartMission \
    -BM_CombatShowcase -BM_CombatShowcaseAutoFire -BM_ReloadPresentationShowcase \
    "-BM_ReloadShowcaseStart=${ENGINE_START_SCREENSHOT}" \
    "-BM_ReloadShowcaseMid=${ENGINE_MID_SCREENSHOT}" \
    "-BM_ReloadShowcaseComplete=${ENGINE_COMPLETE_SCREENSHOT}" \
    -game -NoMutex -nop4 -nosplash -NoSound -NoCrashDialog \
    -windowed -ResX=1280 -ResY=720 -stdout -FullStdOutLogOutput \
    "-abslog=${LOG_PATH}" >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

# Resolve the window before the first stage is requested. The CGWindowList
# probe is compiled on first use; doing that inside the start capture would
# consume most of the short reload presentation interval.
sleep 0.6
resolve_game_window || true

for ((SECOND = 0; SECOND < MAX_SECONDS * 10; ++SECOND)); do
    if (( ! CAPTURED_START )) && rg -q "reload presentation showcase: screenshot requested stage=start" "${STDOUT_PATH}" 2>/dev/null; then
        sleep 0.05
        capture_game_window start "${START_SCREENSHOT}" || true
        CAPTURED_START=1
    fi
    if (( ! CAPTURED_MID )) && rg -q "reload presentation showcase: screenshot requested stage=mid" "${STDOUT_PATH}" 2>/dev/null; then
        sleep 0.05
        capture_game_window mid "${MID_SCREENSHOT}" || true
        CAPTURED_MID=1
    fi
    if (( ! CAPTURED_COMPLETE )) && rg -q "reload presentation showcase: screenshot requested stage=complete" "${STDOUT_PATH}" 2>/dev/null; then
        sleep 0.05
        capture_game_window complete "${COMPLETE_SCREENSHOT}" || true
        CAPTURED_COMPLETE=1
    fi
    if rg -q "Black Meridian reload presentation showcase: PASS" "${STDOUT_PATH}" 2>/dev/null; then
        break
    fi
    if rg -q "Black Meridian reload presentation showcase: FAIL" "${STDOUT_PATH}" 2>/dev/null; then
        rg "Black Meridian reload presentation showcase:" "${STDOUT_PATH}" >&2 || true
        exit 1
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        echo "Packaged reload presentation probe exited before completion" >&2
        rg "Black Meridian reload presentation showcase:" "${STDOUT_PATH}" >&2 || true
        exit 1
    fi
    sleep 0.1
done

if ! rg -q "Black Meridian reload presentation showcase: PASS" "${STDOUT_PATH}" 2>/dev/null; then
    echo "Packaged reload presentation probe timed out after ${MAX_SECONDS}s" >&2
    rg "Black Meridian reload presentation showcase:" "${STDOUT_PATH}" >&2 || true
    exit 1
fi

if ! rg -q "Black Meridian HUD: draw .* screen=10 " "${STDOUT_PATH}" 2>/dev/null; then
    echo "Packaged reload presentation probe never rendered the live gameplay HUD" >&2
    rg "Black Meridian (UI: mission presentation|HUD: draw|reload presentation showcase):" "${STDOUT_PATH}" >&2 || true
    exit 1
fi

for ((SECOND = 0; SECOND < 10; ++SECOND)); do
    if [[ -s "${START_SCREENSHOT}" && -s "${MID_SCREENSHOT}" && -s "${COMPLETE_SCREENSHOT}" ]]; then
        rg "Black Meridian reload presentation showcase:" "${STDOUT_PATH}"
        echo "packaged reload presentation probe passed: ${REPORT_DIR}"
        exit 0
    fi
    sleep 1
done

echo "Reload presentation passed in logs but one or more screenshots were not written" >&2
rg "Black Meridian reload presentation showcase:" "${STDOUT_PATH}" >&2 || true
exit 1
