#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/persistence-probe}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
RUN_ID="$(date +%Y%m%d-%H%M%S)-$$"
SLOT="BlackMeridian_PersistenceProbe_${RUN_ID}"
PROFILE_SLOT="BlackMeridian_ProfileProbe_${RUN_ID}"
WRITE_LOG="${REPORT_DIR}/PersistenceWrite-${RUN_ID}.log"
READ_LOG="${REPORT_DIR}/PersistenceRead-${RUN_ID}.log"
WRITE_STDOUT="${REPORT_DIR}/PersistenceWrite-${RUN_ID}.stdout.log"
READ_STDOUT="${REPORT_DIR}/PersistenceRead-${RUN_ID}.stdout.log"
MAX_SECONDS="${MAX_SECONDS:-75}"

case "$(uname -s)" in
    Darwin)
        EDITOR="${UE_EDITOR:-${UE_ROOT}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
        ;;
    Linux)
        EDITOR="${UE_EDITOR:-${UE_ROOT}/Engine/Binaries/Linux/UnrealEditor}"
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

mkdir -p "${REPORT_DIR}"

run_probe_process() {
    local phase_flag="$1"
    local log_path="$2"
    local stdout_path="$3"

    "${EDITOR}" "${PROJECT_PATH}" \
        -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound \
        -BM_AutoStartMission -BM_MissionProbe -BM_MissionTimeScale=10 \
        "-${phase_flag}" \
        "-BM_PersistenceSlot=${SLOT}" \
        "-BM_ProfileSlot=${PROFILE_SLOT}" \
        "-abslog=${log_path}" \
        >"${stdout_path}" 2>&1 &
    echo "$!"
}

wait_for_result() {
    local pid="$1"
    local log_path="$2"
    local expected="$3"
    local label="$4"

    for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
        if rg -q "${expected}" "${log_path}" 2>/dev/null; then
            return 0
        fi
        if ! kill -0 "${pid}" 2>/dev/null; then
            echo "${label} Unreal process exited before the expected result" >&2
            tail -100 "${log_path}" >&2 || true
            return 1
        fi
        sleep 1
    done

    echo "${label} persistence probe timed out after ${MAX_SECONDS}s" >&2
    tail -120 "${log_path}" >&2 || true
    return 1
}

WRITE_PID="$(run_probe_process BM_PersistenceProbeWrite "${WRITE_LOG}" "${WRITE_STDOUT}")"
wait_for_result "${WRITE_PID}" "${WRITE_LOG}" "persistence probe: WRITE PASS" "write"
wait "${WRITE_PID}" 2>/dev/null || true

READ_PID="$(run_probe_process BM_PersistenceProbeRead "${READ_LOG}" "${READ_STDOUT}")"
wait_for_result "${READ_PID}" "${READ_LOG}" "persistence probe: READ PASS" "read"
wait "${READ_PID}" 2>/dev/null || true

echo "Black Meridian cross-session persistence probe passed"
rg "Black Meridian persistence probe: (WRITE PASS|READ PASS)|Black Meridian persistence: (checkpoint|profile) (save|load)" "${WRITE_LOG}" "${READ_LOG}"
