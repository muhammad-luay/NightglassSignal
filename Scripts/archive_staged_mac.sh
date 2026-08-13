#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app"
ARCHIVE_DIR="${1:-${PROJECT_ROOT}/artifacts/package-mac}"
ARCHIVE_APP="${ARCHIVE_DIR}/NightglassSignalDemo.app"

if [[ ! -x "${STAGED_APP}/Contents/MacOS/NightglassSignalDemo" ]]; then
    echo "Staged Mac app is missing: ${STAGED_APP}" >&2
    echo "Run BuildCookRun with -platform=Mac -build -cook -stage first." >&2
    exit 2
fi

if [[ ! -f "${STAGED_APP}/Contents/UE/NightglassSignal/Content/Paks/NightglassSignal-Mac.utoc" ]]; then
    echo "Staged Mac content container is missing: ${STAGED_APP}/Contents/UE/NightglassSignal/Content/Paks" >&2
    exit 2
fi

mkdir -p "${ARCHIVE_DIR}"
ditto "${STAGED_APP}" "${ARCHIVE_APP}"

if [[ ! -f "${ARCHIVE_APP}/Contents/UE/NightglassSignal/Content/Paks/NightglassSignal-Mac.utoc" ]]; then
    echo "Archive copy is incomplete: ${ARCHIVE_APP}" >&2
    exit 1
fi

echo "Archived staged Mac app: ${ARCHIVE_APP}"
du -sh "${ARCHIVE_APP}"
