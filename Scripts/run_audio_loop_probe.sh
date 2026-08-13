#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/audio-loop-probe}"
MAP_PATH="${MAP_PATH:-/Game/Levels/Glasswake}"
PROBE_SECONDS="${PROBE_SECONDS:-30}"

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

APP_BINARY="${STAGED_APP}/Contents/MacOS/NightglassSignalDemo"
if [[ ! -x "${APP_BINARY}" ]]; then
    echo "Packaged executable is missing or not executable: ${APP_BINARY}" >&2
    exit 2
fi

if [[ ! "${PROBE_SECONDS}" =~ ^[0-9]+$ || "${PROBE_SECONDS}" -lt 10 ]]; then
    echo "PROBE_SECONDS must be an integer of at least 10" >&2
    exit 2
fi

mkdir -p "${REPORT_DIR}"
STDOUT_PATH="${REPORT_DIR}/AudioLoopProbe.stdout.log"
RESULT_PATH="${REPORT_DIR}/AudioLoopProbe.result.txt"
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

pushd "${STAGED_APP}/Contents/UE/NightglassSignal/Binaries/Mac" >/dev/null
"${APP_BINARY}" "${MAP_PATH}" \
    -BM_CampaignLevel=0 -BM_AutoStartMission -BM_AudioProbe \
    -game -installed -windowed -ResX=960 -ResY=540 -NoMutex -NoLiveCoding \
    -NoCrashDialog -nosplash -stdout -FullStdOutLogOutput -nop4 \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!
popd >/dev/null

for ((Second = 0; Second < PROBE_SECONDS; ++Second)); do
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        break
    fi
    sleep 1
done

MusicMarkerCount="$(grep -c 'Black Meridian audio: music state=Suspense.*loop=yes' "${STDOUT_PATH}" || true)"
HeartbeatCount="$(grep -c 'Black Meridian audio probe: music heartbeat' "${STDOUT_PATH}" || true)"
PlayingNoCount="$(grep -c 'Black Meridian audio probe: music heartbeat.*playing=no' "${STDOUT_PATH}" || true)"
UnderrunCount="$(grep -c 'Audio Buffer Underrun' "${STDOUT_PATH}" || true)"

if (( MusicMarkerCount < 1 || HeartbeatCount < 2 || PlayingNoCount > 0 )); then
    Result="Black Meridian audio loop probe: FAIL suspense_loop_markers=${MusicMarkerCount} heartbeats=${HeartbeatCount} playing_no=${PlayingNoCount} underruns=${UnderrunCount} duration_s=${PROBE_SECONDS}"
    printf '%s\n' "${Result}" | tee "${RESULT_PATH}" >&2
    exit 1
fi

Result="Black Meridian audio loop probe: PASS suspense_loop_markers=${MusicMarkerCount} heartbeats=${HeartbeatCount} playing_no=${PlayingNoCount} underruns=${UnderrunCount} duration_s=${PROBE_SECONDS}"
printf '%s\n' "${Result}" | tee "${RESULT_PATH}"
