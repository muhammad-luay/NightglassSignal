#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/automation}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
case "$(uname -s)" in
    Darwin)
        # The installed UE5.8 Mac distribution runs the command-line editor
        # reliably through the app bundle executable; its standalone command
        # alias exits before report creation on this distribution.
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

mkdir -p "${REPORT_DIR}"
LOG_PATH="${LOG_PATH:-${REPORT_DIR}/BlackMeridian.log}"
"${EDITOR}" "${PROJECT_PATH}" \
    -NoMutex -NoLiveCoding -NoSound \
    -unattended -nop4 -nosplash -nullrhi \
    -ExecCmds="Automation RunTests BlackMeridian; Quit" \
    "-ReportExportPath=${REPORT_DIR}" \
    "-abslog=${LOG_PATH}"
