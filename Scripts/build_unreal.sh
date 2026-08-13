#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine 5.1 installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
PLATFORM="${PLATFORM:-Win64}"
CONFIGURATION="${CONFIGURATION:-Development}"
ARCHIVE_DIR="${ARCHIVE_DIR:-${PROJECT_ROOT}/artifacts/package}"
BUILD_SERVER="${BUILD_SERVER:-auto}"
GAME_TARGET="${GAME_TARGET:-NightglassSignalDemo}"
UBT_ARGS="${UBT_ARGS:-}"
UAT="${UE_ROOT}/Engine/Build/BatchFiles/RunUAT.sh"

if [[ ! -x "${UAT}" ]]; then
    echo "RunUAT.sh not found or not executable: ${UAT}" >&2
    exit 2
fi

COMMON_UAT_ARGS=(
    BuildCookRun
    "-project=${PROJECT_PATH}"
    -noP4
    "-clientconfig=${CONFIGURATION}"
    "-platform=${PLATFORM}"
)

# Allow recovery from a stale/editor-held UBT mutex without changing the
# normal single-instance default. This is also useful when a packaged client
# and an editor are being inspected at the same time.
if [[ -n "${UBT_ARGS}" ]]; then
    COMMON_UAT_ARGS+=("-ubtargs=${UBT_ARGS}")
fi

# Epic's Mac editor distribution does not ship a dedicated-server target.
# A standalone client is sufficient for the local playable build and the
# packaged gameplay probes. Keep the old server behavior for platforms where
# the target is available, while allowing an explicit override for custom
# engine installations.
if [[ "${BUILD_SERVER}" == "1" || ( "${BUILD_SERVER}" == "auto" && "${PLATFORM}" != "Mac" ) ]]; then
    COMMON_UAT_ARGS+=(
        -server
        "-serverconfig=${CONFIGURATION}"
    )
else
    echo "Skipping dedicated server target for platform=${PLATFORM} (BUILD_SERVER=${BUILD_SERVER})"
fi

mkdir -p "${ARCHIVE_DIR}"

if [[ "${PLATFORM}" == "Mac" ]]; then
    # UE 5.8's Mac post-build step emits an .app bundle, while the generic
    # BuildCookRun staging manifest still expects the receipt's old loose
    # executable path. Build/cook first, expose that executable as a temporary
    # hard link for the manifest, then let Xcode create the real staged app.
    # The final archive is copied from Saved/StagedBuilds because UAT's generic
    # archive step can select the pre-stage Binaries app wrapper.
    "${UAT}" "${COMMON_UAT_ARGS[@]}" -build -cook

    APP_BINARY="${PROJECT_ROOT}/Binaries/Mac/${GAME_TARGET}.app/Contents/MacOS/${GAME_TARGET}"
    LOOSE_BINARY="${PROJECT_ROOT}/Binaries/Mac/${GAME_TARGET}"
    STAGED_APP="${PROJECT_ROOT}/Saved/StagedBuilds/Mac/${GAME_TARGET}.app"
    ARCHIVE_APP="${ARCHIVE_DIR}/${GAME_TARGET}.app"
    if [[ ! -x "${APP_BINARY}" ]]; then
        echo "Mac app executable not found after build: ${APP_BINARY}" >&2
        exit 1
    fi

    CREATED_LOOSE_BINARY=0
    cleanup_loose_binary() {
        if [[ "${CREATED_LOOSE_BINARY}" == "1" && -e "${LOOSE_BINARY}" ]]; then
            rm -f "${LOOSE_BINARY}"
        fi
    }
    trap cleanup_loose_binary EXIT

    # A prior compatibility run may have left a symlink here. It is not safe
    # for Xcode's staging copy because the relative link breaks inside .app.
    if [[ -L "${LOOSE_BINARY}" ]]; then
        rm -f "${LOOSE_BINARY}"
    fi
    if [[ ! -e "${LOOSE_BINARY}" ]]; then
        ln "${APP_BINARY}" "${LOOSE_BINARY}"
        CREATED_LOOSE_BINARY=1
    fi

    "${UAT}" "${COMMON_UAT_ARGS[@]}" -skipbuild -skipcook -stage -pak

    if [[ ! -x "${STAGED_APP}/Contents/MacOS/${GAME_TARGET}" ]]; then
        echo "Staged Mac app executable is missing: ${STAGED_APP}" >&2
        exit 1
    fi
    if [[ ! -f "${STAGED_APP}/Contents/UE/${PROJECT_ROOT##*/}/Content/Paks/${PROJECT_ROOT##*/}-Mac.utoc" ]]; then
        echo "Staged Mac content container is missing: ${STAGED_APP}" >&2
        exit 1
    fi

    rm -rf "${ARCHIVE_APP}"
    ditto "${STAGED_APP}" "${ARCHIVE_APP}"
    echo "Archived staged Mac app: ${ARCHIVE_APP}"
else
    "${UAT}" "${COMMON_UAT_ARGS[@]}" -build -cook -stage -pak -archive \
        "-archivedirectory=${ARCHIVE_DIR}"
fi
