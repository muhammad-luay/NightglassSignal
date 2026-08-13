#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
REPORT_ROOT="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/animation-dispatch-probe}"
MAP_PATH="${MAP_PATH:-/Game/Levels/Glasswake}"
MAX_SECONDS="${MAX_SECONDS:-30}"

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
STDOUT_PATH="${REPORT_DIR}/AnimationDispatch.stdout.log"
LOG_PATH="${REPORT_DIR}/AnimationDispatch.log"
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

"${EDITOR}" "${PROJECT_PATH}" "${MAP_PATH}" \
    -BM_CampaignLevel=0 -BM_CombatShowcase -BM_CombatShowcaseAutoFire \
    -BM_AnimationDispatchProbe -game -windowed -ResX=1280 -ResY=720 \
    -NoMutex -NoLiveCoding -NoCrashDialog -nosplash -nop4 -nullrhi -NoSound \
    -stdout -FullStdOutLogOutput -unattended "-abslog=${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if rg -q "Black Meridian presentation: project AnimBP route pawn=BMEnemyCharacter_.* active=yes" "${STDOUT_PATH}" 2>/dev/null \
        && rg -q "Black Meridian presentation: fire animation kept on locomotion pawn=BMEnemyCharacter_" "${STDOUT_PATH}" 2>/dev/null \
        && rg -q "Black Meridian presentation: authored combat animation event pawn=BMEnemyCharacter_.* event=HitReact .*mode=montage additive=yes" "${STDOUT_PATH}" 2>/dev/null \
        && rg -q "Black Meridian presentation: additive reaction layered pawn=BMEnemyCharacter_.* event=HitReact locomotion_layered=yes" "${STDOUT_PATH}" 2>/dev/null \
        && rg -q "Black Meridian presentation: hit feedback route pawn=BMEnemyCharacter_" "${STDOUT_PATH}" 2>/dev/null \
        && rg -q "Black Meridian presentation: authored combat animation event pawn=BMEnemyCharacter_.* event=Death .*additive=no" "${STDOUT_PATH}" 2>/dev/null \
        && rg -q "Black Meridian presentation: death knockdown pawn=BMEnemyCharacter_" "${STDOUT_PATH}" 2>/dev/null; then
        echo "Black Meridian animation dispatch probe passed: ${REPORT_DIR}"
        rg "Black Meridian presentation: (project AnimBP route pawn=BMEnemyCharacter_|authored combat animation event pawn=BMEnemyCharacter_|hit feedback route pawn=BMEnemyCharacter_|death knockdown pawn=BMEnemyCharacter_)" "${STDOUT_PATH}" | tail -60
        exit 0
    fi
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        break
    fi
    sleep 1
done

echo "Black Meridian animation dispatch probe failed after ${MAX_SECONDS}s" >&2
rg "Black Meridian presentation: (project mannequin rifle events|project AnimBP route pawn=BMEnemyCharacter_|fire animation kept on locomotion pawn=BMEnemyCharacter_|authored combat animation event pawn=BMEnemyCharacter_|additive reaction layered pawn=BMEnemyCharacter_|hit feedback route pawn=BMEnemyCharacter_|death knockdown pawn=BMEnemyCharacter_)" "${STDOUT_PATH}" >&2 || true
exit 1
