#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
REPORT_ROOT="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/crosshair-probe}"
MAP_PATH="${MAP_PATH:-/Game/Levels/Glasswake}"
MAX_SECONDS="${MAX_SECONDS:-30}"
# This probe validates a player-visible UMG reticle.  A null RHI does not
# create a drawable HUD canvas, so it can only validate gameplay state and
# would make a visual target probe report a false negative.
NULL_RHI="${NULL_RHI:-0}"

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
REPORT_DIR="${REPORT_ROOT}/${RUN_ID}"
mkdir -p "${REPORT_DIR}"
STDOUT_PATH="${REPORT_DIR}/Crosshair.stdout.log"
LOG_PATH="${REPORT_DIR}/Crosshair.log"
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

RHI_ARGS=(-NoSound)
if [[ "${NULL_RHI}" == "1" ]]; then
    RHI_ARGS+=(-nullrhi)
fi

"${EDITOR}" "${PROJECT_PATH}" "${MAP_PATH}" \
    -BM_CampaignLevel=0 -BM_CombatShowcase -BM_CombatCloseFacingProbe \
    -BM_CrosshairProbe \
    -game -windowed -ResX=1280 -ResY=720 \
    -NoMutex -NoLiveCoding -NoCrashDialog -nosplash -nop4 "${RHI_ARGS[@]}" \
    -stdout -FullStdOutLogOutput -unattended "-abslog=${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian UI: native crosshair target live_enemy=yes color=danger_red" "${STDOUT_PATH}" 2>/dev/null; then
        echo "Black Meridian native crosshair probe passed: ${REPORT_DIR}"
        rg "Black Meridian UI: native crosshair target" "${STDOUT_PATH}" | tail -20
        exit 0
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        break
    fi
    sleep 1
done

echo "Black Meridian native crosshair probe failed after ${MAX_SECONDS}s" >&2
rg "Black Meridian UI: native crosshair target|Black Meridian showcase: visibility" "${STDOUT_PATH}" >&2 || true
exit 1
