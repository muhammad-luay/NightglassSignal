#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_PATH="${APP_PATH:-${PROJECT_ROOT}/Binaries/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/performance-probe}"
DURATION="${DURATION:-30}"

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

if [[ ! -x "${APP_PATH}/Contents/MacOS/NightglassSignalDemo" ]]; then
    echo "Mac game app not found: ${APP_PATH}" >&2
    echo "Build it first with UE_ROOT=/Users/Shared/Epic\\ Games/UE_5.8 PLATFORM=Mac Scripts/build_unreal.sh" >&2
    exit 2
fi

mkdir -p "${REPORT_DIR}"
LOG_PATH="${REPORT_DIR}/PerformanceProbe.log"
STDOUT_PATH="${REPORT_DIR}/PerformanceProbe.stdout.log"
APP_BINARY="${APP_PATH}/Contents/MacOS/NightglassSignalDemo"
pushd "${APP_PATH}/Contents/UE/NightglassSignal/Binaries/Mac" >/dev/null
set +e
"${APP_BINARY}" \
    -windowed -ResX=1920 -ResY=1080 -NoSound \
    -installed \
    -NoMutex -NoLiveCoding -NoCrashDialog -nosplash \
    -stdout -FullStdOutLogOutput \
    -name=NightglassPerformanceProbe \
    -BM_AutoStartMission \
    -BM_PerformanceProbe \
    "-BM_PerformanceProbeSeconds=${DURATION}" \
    -BM_PerformanceProbeExit \
    -log "-abslog=${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1
STATUS=$?
popd >/dev/null

# Some staged Mac builds route UE's absolute log sink through the app
# container even when -abslog is supplied. Preserve the complete captured
# stdout stream at the documented log path so every run has one stable
# evidence file.
if [[ ! -s "${LOG_PATH}" ]]; then
    cp "${STDOUT_PATH}" "${LOG_PATH}"
fi
exit "${STATUS}"
