#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    echo "UE_ROOT must point to a provisioned Unreal Engine installation" >&2
    exit 2
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="${PROJECT_PATH:-${PROJECT_ROOT}/NightglassSignal.uproject}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/combat-probe}"
if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi
PORT="${PORT:-7788}"
STARTUP_SECONDS="${STARTUP_SECONDS:-60}"
MAX_SECONDS="${MAX_SECONDS:-90}"
NET_EMULATION_ARGS=()
if [[ -n "${PKT_LAG:-}" ]]; then
    NET_EMULATION_ARGS+=("-PktLag=${PKT_LAG}")
fi
if [[ -n "${PKT_LOSS:-}" ]]; then
    NET_EMULATION_ARGS+=("-PktLoss=${PKT_LOSS}")
fi
DEATH_PRESENTATION_ARGS=()
if [[ -n "${DEATH_PRESENTATION_PROBE:-}" ]]; then
    DEATH_PRESENTATION_ARGS+=("-BM_DeathPresentationProbe")
fi
RELOAD_PROBE_ARGS=()
RELOAD_PROBE="${RELOAD_PROBE:-0}"
RELOAD_INTERRUPT="${RELOAD_INTERRUPT:-1}"
RELOAD_PROBE_EMPTY="${RELOAD_PROBE_EMPTY:-0}"
if [[ "${RELOAD_PROBE_EMPTY}" != "0" && "${RELOAD_PROBE_EMPTY}" != "1" ]]; then
    echo "RELOAD_PROBE_EMPTY must be 0 or 1" >&2
    exit 2
fi
if [[ "${RELOAD_PROBE_EMPTY}" == "1" ]]; then
    RELOAD_PROBE=1
    RELOAD_INTERRUPT=0
fi
if [[ "${RELOAD_PROBE}" == "1" ]]; then
    if [[ "${RELOAD_INTERRUPT}" != "0" && "${RELOAD_INTERRUPT}" != "1" ]]; then
        echo "RELOAD_INTERRUPT must be 0 or 1" >&2
        exit 2
    fi
    RELOAD_PROBE_ARGS+=("-BM_ReloadProbe")
    if [[ "${RELOAD_PROBE_EMPTY}" == "1" ]]; then
        RELOAD_PROBE_ARGS+=("-BM_ReloadProbeEmpty")
    fi
    if [[ "${RELOAD_INTERRUPT}" == "0" ]]; then
        RELOAD_PROBE_ARGS+=("-BM_ReloadProbeNoInterrupt")
    fi
fi
	AIM_FALLBACK_ARGS=()
AIM_FALLBACK_PROBE="${AIM_FALLBACK_PROBE:-0}"
if [[ "${AIM_FALLBACK_PROBE}" != "0" && "${AIM_FALLBACK_PROBE}" != "1" ]]; then
    echo "AIM_FALLBACK_PROBE must be 0 or 1" >&2
    exit 2
fi
if [[ "${AIM_FALLBACK_PROBE}" == "1" ]]; then
    AIM_FALLBACK_ARGS+=("-BM_CombatFallbackProbe")
fi
REQUIRE_REWIND_PROBE=0
if [[ -n "${PKT_LAG:-}" ]]; then
    REQUIRE_REWIND_PROBE=1
fi
RUN_ID="$(date +%Y%m%d-%H%M%S)"
HOST_LOG="${REPORT_DIR}/CombatHost-${RUN_ID}.log"
CLIENT_A_LOG="${REPORT_DIR}/CombatClientA-${RUN_ID}.log"
CLIENT_B_LOG="${REPORT_DIR}/CombatClientB-${RUN_ID}.log"
HOST_STDOUT="${REPORT_DIR}/CombatHost-${RUN_ID}.stdout.log"
CLIENT_A_STDOUT="${REPORT_DIR}/CombatClientA-${RUN_ID}.stdout.log"
CLIENT_B_STDOUT="${REPORT_DIR}/CombatClientB-${RUN_ID}.stdout.log"

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

if command -v lsof >/dev/null 2>&1; then
    LISTENERS="$(lsof -nP -iTCP:"${PORT}" -sTCP:LISTEN 2>/dev/null || true)"
    if [[ -n "${LISTENERS}" ]]; then
        echo "Combat probe port ${PORT} is already in use; refusing to stop the existing listener:" >&2
        echo "${LISTENERS}" >&2
        exit 1
    fi
fi

mkdir -p "${REPORT_DIR}"
HOST_PID=""
CLIENT_A_PID=""
CLIENT_B_PID=""
HOST_BOUND_PORT=""

cleanup() {
    for PID in "${CLIENT_B_PID}" "${CLIENT_A_PID}" "${HOST_PID}"; do
        if [[ -n "${PID}" ]] && kill -0 "${PID}" 2>/dev/null; then
            kill "${PID}" 2>/dev/null || true
        fi
    done
    for PID in "${CLIENT_B_PID}" "${CLIENT_A_PID}" "${HOST_PID}"; do
        if [[ -n "${PID}" ]]; then
            wait "${PID}" 2>/dev/null || true
        fi
    done
}
trap cleanup EXIT INT TERM

"${EDITOR}" "${PROJECT_PATH}" \
    "/Engine/Maps/Entry?game=/Script/BlackMeridianRuntime.BMSignalControlGameMode?listen" \
    "${NET_EMULATION_ARGS[@]+${NET_EMULATION_ARGS[@]}}" \
    "${DEATH_PRESENTATION_ARGS[@]+${DEATH_PRESENTATION_ARGS[@]}}" \
    "${RELOAD_PROBE_ARGS[@]+${RELOAD_PROBE_ARGS[@]}}" \
    "${AIM_FALLBACK_ARGS[@]+${AIM_FALLBACK_ARGS[@]}}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_AutoStartSignalMatch -BM_SignalControlProbe -BM_CombatProbe -port="${PORT}" \
    "-abslog=${HOST_LOG}" \
    >"${HOST_STDOUT}" 2>&1 &
HOST_PID=$!

# Unreal may silently advance to the next port when the requested bind fails.
# Do not let the clients connect to an unrelated listener in that case: the
# resulting logs can contain perfectly valid replicated combat while the
# authoritative probe state machine never sees the clients that were meant to
# drive it.
for ((Second = 0; Second < STARTUP_SECONDS; ++Second)); do
    if rg -q "IpNetDriver listening on port [0-9]+" "${HOST_LOG}" 2>/dev/null; then
        HOST_BOUND_PORT="$(rg -o "IpNetDriver listening on port [0-9]+" "${HOST_LOG}" | tail -1 | awk '{print $NF}')"
        if [[ "${HOST_BOUND_PORT}" != "${PORT}" ]]; then
            echo "Combat probe host bound to port ${HOST_BOUND_PORT}, requested ${PORT}; refusing to attach clients to a different session" >&2
            tail -80 "${HOST_LOG}" >&2 || true
            exit 1
        fi
        break
    fi
    if ! kill -0 "${HOST_PID}" 2>/dev/null; then
        echo "Combat probe host exited before binding its requested port" >&2
        tail -120 "${HOST_LOG}" 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

if [[ "${HOST_BOUND_PORT}" != "${PORT}" ]]; then
    echo "Combat probe host did not bind requested port ${PORT} within ${STARTUP_SECONDS}s" >&2
    tail -120 "${HOST_LOG}" 2>/dev/null || true
    exit 1
fi

for ((Second = 0; Second < STARTUP_SECONDS; ++Second)); do
    if rg -q "Black Meridian signal: authoritative countdown started" "${HOST_LOG}" 2>/dev/null; then
        break
    fi
    if ! kill -0 "${HOST_PID}" 2>/dev/null; then
        echo "Combat probe host exited before opening the match" >&2
        tail -120 "${HOST_LOG}" 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

if ! rg -q "Black Meridian signal: authoritative countdown started" "${HOST_LOG}" 2>/dev/null; then
    echo "Combat probe host did not start its authoritative countdown within ${STARTUP_SECONDS}s" >&2
    tail -120 "${HOST_LOG}" 2>/dev/null || true
    exit 1
fi

wait_for_client_handshake() {
    local CLIENT_PID="$1"
    local CLIENT_LOG="$2"
    local CLIENT_LABEL="$3"
    for ((Second = 0; Second < STARTUP_SECONDS; ++Second)); do
        if rg -q "LogNet: Welcomed by server" "${CLIENT_LOG}" 2>/dev/null; then
            return 0
        fi
        if ! kill -0 "${CLIENT_PID}" 2>/dev/null; then
            echo "Combat probe ${CLIENT_LABEL} exited before completing its network handshake" >&2
            tail -100 "${CLIENT_LOG}" 2>/dev/null || true
            return 1
        fi
        sleep 1
    done
    echo "Combat probe ${CLIENT_LABEL} did not complete its network handshake within ${STARTUP_SECONDS}s" >&2
    tail -100 "${CLIENT_LOG}" 2>/dev/null || true
    return 1
}

"${EDITOR}" "${PROJECT_PATH}" "127.0.0.1:${PORT}" \
    "${NET_EMULATION_ARGS[@]+${NET_EMULATION_ARGS[@]}}" \
    "${DEATH_PRESENTATION_ARGS[@]+${DEATH_PRESENTATION_ARGS[@]}}" \
    "${RELOAD_PROBE_ARGS[@]+${RELOAD_PROBE_ARGS[@]}}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_SignalControlProbe -BM_CombatProbe -name=CombatProbeClientA \
    "-abslog=${CLIENT_A_LOG}" \
    >"${CLIENT_A_STDOUT}" 2>&1 &
CLIENT_A_PID=$!

# Starting both Mac editor clients at once can make their startup UBT probes
# contend for Trace.uba. Wait for the first client to be welcomed before
# creating the second process so the evidence reflects gameplay rather than a
# toolchain startup race.
wait_for_client_handshake "${CLIENT_A_PID}" "${CLIENT_A_LOG}" "client A"

"${EDITOR}" "${PROJECT_PATH}" "127.0.0.1:${PORT}" \
    "${NET_EMULATION_ARGS[@]+${NET_EMULATION_ARGS[@]}}" \
    "${DEATH_PRESENTATION_ARGS[@]+${DEATH_PRESENTATION_ARGS[@]}}" \
    "${RELOAD_PROBE_ARGS[@]+${RELOAD_PROBE_ARGS[@]}}" \
    -game -NoMutex -unattended -nop4 -nosplash -nullrhi -NoSound -NoCrashDialog \
    -BM_SignalControlProbe -BM_CombatProbe -name=CombatProbeClientB \
    "-abslog=${CLIENT_B_LOG}" \
    >"${CLIENT_B_STDOUT}" 2>&1 &
CLIENT_B_PID=$!

DISCONNECT_REQUESTED=0
has_probe_client_fire() {
    local CLIENT_LOG="$1"
    rg -q "Black Meridian combat probe: client (fire dispatch|weapon fire)" "${CLIENT_LOG}" 2>/dev/null
}

has_reload_probe_passed() {
    if [[ "${RELOAD_PROBE}" != "1" ]]; then
        return 0
    fi
    rg -q "Black Meridian combat probe: reload probe passed interruption=(yes|no) completion=yes" "${HOST_LOG}" 2>/dev/null
}

has_rewind_probe_passed() {
    if [[ "${REQUIRE_REWIND_PROBE}" != "1" ]]; then
        return 0
    fi
    # A packet-emulated remote client must produce a nonzero server RTT,
    # bounded rewind, and at least one rewound target. Local bot fire is
    # allowed to report ping=0/targets=0; it is not the acceptance signal.
    rg -q "Black Meridian combat rewind: .*ping_ms=[1-9][0-9]*\\.[0-9]+ rewind_ms=[1-9][0-9]*\\.[0-9]+ targets=[1-9][0-9]*" "${HOST_LOG}" 2>/dev/null
}

has_aim_fallback_probe_passed() {
    if [[ "${AIM_FALLBACK_PROBE}" != "1" ]]; then
        return 0
    fi
    rg -q "Black Meridian combat validation: current_pose_fallback target=" "${HOST_LOG}" 2>/dev/null
}

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if ! kill -0 "${HOST_PID}" 2>/dev/null; then
        echo "Combat probe host exited before probe completion" >&2
        tail -160 "${HOST_LOG}" 2>/dev/null || true
        exit 1
    fi

    if [[ -n "${DEATH_PRESENTATION_PROBE:-}" ]] \
        && rg -q "Black Meridian death presentation probe: server eliminated target=.*remote_controller=yes" "${HOST_LOG}" \
        && (rg -q "Black Meridian presentation: death knockdown .*authority=no" "${CLIENT_A_LOG}" 2>/dev/null \
            || rg -q "Black Meridian presentation: death knockdown .*authority=no" "${CLIENT_B_LOG}" 2>/dev/null); then
        echo "Black Meridian death presentation probe passed"
        rg "Black Meridian (death presentation probe|presentation: death knockdown)" "${HOST_LOG}" "${CLIENT_A_LOG}" "${CLIENT_B_LOG}" 2>/dev/null || true
        exit 0
    fi

    if [[ "${DISCONNECT_REQUESTED}" -eq 0 ]] \
        && rg -q "Black Meridian combat probe: server damage verified" "${HOST_LOG}" \
        && rg -q "Black Meridian combat (probe|trace): server shot accepted" "${HOST_LOG}" \
        && (has_probe_client_fire "${CLIENT_A_LOG}" || has_probe_client_fire "${CLIENT_B_LOG}") \
        && (rg -q "Black Meridian combat probe: client ammo replicated .*magazine=" "${CLIENT_A_LOG}" 2>/dev/null || rg -q "Black Meridian combat probe: client ammo replicated .*magazine=" "${CLIENT_B_LOG}" 2>/dev/null) \
        && (rg -q "Black Meridian combat probe: client health replicated .*health=" "${CLIENT_A_LOG}" 2>/dev/null || rg -q "Black Meridian combat probe: client health replicated .*health=" "${CLIENT_B_LOG}" 2>/dev/null) \
        && has_reload_probe_passed \
        && has_aim_fallback_probe_passed \
        && has_rewind_probe_passed; then
        if has_probe_client_fire "${CLIENT_A_LOG}"; then
            PROBE_CLIENT_PID="${CLIENT_A_PID}"
            CLIENT_A_PID=""
        else
            PROBE_CLIENT_PID="${CLIENT_B_PID}"
            CLIENT_B_PID=""
        fi
        if [[ "${RELOAD_PROBE}" == "1" ]]; then
            rg "Black Meridian combat probe: (reload|server dispatched client reload)" "${HOST_LOG}" "${CLIENT_A_LOG}" "${CLIENT_B_LOG}" 2>/dev/null || true
        fi
        if [[ "${REQUIRE_REWIND_PROBE}" == "1" ]]; then
            rg "Black Meridian combat rewind:" "${HOST_LOG}" 2>/dev/null || true
        fi
        if [[ "${AIM_FALLBACK_PROBE}" == "1" ]]; then
            rg "Black Meridian combat fallback probe:|Black Meridian combat validation: current_pose_fallback|Black Meridian combat probe: server damage verified" "${HOST_LOG}" 2>/dev/null || true
        fi
        echo "Terminating probe-owned shooter client PID ${PROBE_CLIENT_PID} after authoritative combat evidence"
        kill "${PROBE_CLIENT_PID}" 2>/dev/null || true
        wait "${PROBE_CLIENT_PID}" 2>/dev/null || true
        DISCONNECT_REQUESTED=1
    fi

    if [[ "${DISCONNECT_REQUESTED}" -eq 1 ]] \
        && rg -q "Black Meridian combat probe: disconnect observed .*bot_takeover=yes" "${HOST_LOG}" 2>/dev/null; then
        echo "Black Meridian combat/network probe passed"
        rg "Black Meridian combat probe:" "${HOST_LOG}" || true
        echo "-- client A --"
        rg "Black Meridian combat probe:" "${CLIENT_A_LOG}" 2>/dev/null || true
        echo "-- client B --"
        rg "Black Meridian combat probe:" "${CLIENT_B_LOG}" 2>/dev/null || true
        exit 0
    fi

    if [[ -n "${CLIENT_A_PID}" ]] && ! kill -0 "${CLIENT_A_PID}" 2>/dev/null; then
        echo "Combat probe client A exited before evidence was complete" >&2
        tail -100 "${CLIENT_A_LOG}" 2>/dev/null || true
        exit 1
    fi
    if [[ -n "${CLIENT_B_PID}" ]] && ! kill -0 "${CLIENT_B_PID}" 2>/dev/null; then
        echo "Combat probe client B exited before evidence was complete" >&2
        tail -100 "${CLIENT_B_LOG}" 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

echo "Combat probe timed out after ${MAX_SECONDS}s" >&2
rg "Black Meridian combat probe:" "${HOST_LOG}" >&2 || true
rg "Black Meridian combat probe:" "${CLIENT_A_LOG}" >&2 || true
rg "Black Meridian combat probe:" "${CLIENT_B_LOG}" >&2 || true
exit 1
