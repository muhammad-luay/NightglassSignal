#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGED_APP="${STAGED_APP:-${PROJECT_ROOT}/Saved/StagedBuilds/Mac/NightglassSignalDemo.app}"
REPORT_DIR="${REPORT_DIR:-${PROJECT_ROOT}/artifacts/combat-role-probe}"
MAP_PATH="${MAP_PATH:-/Game/Levels/Glasswake}"
MAX_SECONDS="${MAX_SECONDS:-20}"

if [[ "${REPORT_DIR}" != /* ]]; then
    REPORT_DIR="${PROJECT_ROOT}/${REPORT_DIR}"
fi

EXECUTABLE="${STAGED_APP}/Contents/MacOS/NightglassSignalDemo"
if [[ ! -x "${EXECUTABLE}" ]]; then
    echo "Packaged executable is missing or not executable: ${EXECUTABLE}" >&2
    exit 2
fi

mkdir -p "${REPORT_DIR}"
STDOUT_PATH="${REPORT_DIR}/CombatRoleProbe.stdout.log"
LOG_PATH="${REPORT_DIR}/CombatRoleProbe.log"
RESULT_PATH="${REPORT_DIR}/CombatRoleProbe.result.txt"
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

"${EXECUTABLE}" "${MAP_PATH}" \
    -BM_CampaignLevel=0 -BM_CombatShowcase -BM_CombatRoleProbe \
    -game -windowed -ResX=1280 -ResY=720 -NoMutex -NoLiveCoding \
    -NoCrashDialog -nosplash -stdout -FullStdOutLogOutput -NoSound -nullrhi \
    -abslog="${LOG_PATH}" \
    >"${STDOUT_PATH}" 2>&1 &
GAME_PID=$!

for ((Second = 0; Second < MAX_SECONDS; ++Second)); do
    if ! kill -0 "${GAME_PID}" 2>/dev/null; then
        break
    fi
    sleep 1
done

if [[ ! -s "${STDOUT_PATH}" ]]; then
    echo "Combat role probe produced no stdout: ${STDOUT_PATH}" >&2
    exit 1
fi

EnemyCount="$(rg -c "Black Meridian showcase: enemy index=" "${STDOUT_PATH}" || true)"
# Navigation is now expected to be available for the runtime-generated
# layouts. Keep the compatibility fallback accepted for deliberately
# navigation-less sessions, but count either real movement path as evidence.
MovementCount="$(rg -c "Black Meridian AI movement: (navigation path active|direct fallback active)" "${STDOUT_PATH}" || true)"
FireCount="$(rg -c "Black Meridian role fire: pawn=.* role=" "${STDOUT_PATH}" || true)"
RoleLabels="$(rg -o "role=(ASSAULT|BREACHER|RIFLEMAN|MARKSMAN|DEMOLITIONIST)" "${STDOUT_PATH}" | sort -u || true)"
RoleCount="$(printf '%s\n' "${RoleLabels}" | sed '/^$/d' | wc -l | tr -d ' ')"
DemolitionistRuntimeMatch=0
if rg -q "Black Meridian showcase: enemy index=.* role=DEMOLITIONIST .* weapon=5 preferred=820 engage=1180 speed=145 " "${STDOUT_PATH}"; then
    DemolitionistRuntimeMatch=1
fi
FacingMismatchCount="$(awk '
    {
        MovementActive = 0
        MovementYaw = ""
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "movement_active=yes") {
                MovementActive = 1
            } else if ($Index ~ /^body_to_movement_yaw=/) {
                MovementYaw = $Index
                sub(/^body_to_movement_yaw=/, "", MovementYaw)
            }
        }
        TargetFacing = 0
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "target_facing=yes") {
                TargetFacing = 1
            }
        }
        if (MovementActive && !TargetFacing && MovementYaw != "" && ((MovementYaw + 0) > 45 || (MovementYaw + 0) < -45)) {
            Count++
        }
    }
    END { print Count + 0 }
' "${STDOUT_PATH}")"
FacingSampleCount="$(rg -c "body_to_movement_yaw=" "${STDOUT_PATH}" || true)"
TargetFacingMismatchCount="$(awk '
    {
        TargetFacing = 0
        MovementActive = 0
        TargetYaw = ""
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "target_facing=yes") {
                TargetFacing = 1
            } else if ($Index == "movement_active=yes") {
                MovementActive = 1
            } else if ($Index ~ /^visual_to_target_yaw=/) {
                TargetYaw = $Index
                sub(/^visual_to_target_yaw=/, "", TargetYaw)
            }
        }
        # The rendered mesh must face the live target while moving as well as
        # while stopped. The capsule may keep a route-facing yaw for collision
        # and path following, so validate the rendered visual heading here.
        if (TargetFacing && TargetYaw != "" && ((TargetYaw + 0) > 45 || (TargetYaw + 0) < -45)) {
            Count++
        }
    }
    END { print Count + 0 }
' "${STDOUT_PATH}")"
TargetFacingSampleCount="$(rg -c "visual_to_target_yaw=" "${STDOUT_PATH}" || true)"
ForwardMovementMismatchCount="$(awk '
    {
        MovementActive = 0
        MovementYaw = ""
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "movement_active=yes") {
                MovementActive = 1
            } else if ($Index ~ /^body_to_movement_yaw=/) {
                MovementYaw = $Index
                sub(/^body_to_movement_yaw=/, "", MovementYaw)
            }
        }
        TargetFacing = 0
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "target_facing=yes") {
                TargetFacing = 1
            }
        }
        if (MovementActive && !TargetFacing && MovementYaw != "" && ((MovementYaw + 0) > 80 || (MovementYaw + 0) < -80)) {
            Count++
        }
    }
    END { print Count + 0 }
' "${STDOUT_PATH}")"
VisualMovementMismatchCount="$(awk '
    {
        MovementActive = 0
        VisualMovementYaw = ""
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "movement_active=yes") {
                MovementActive = 1
            } else if ($Index ~ /^visual_to_movement_yaw=/) {
                VisualMovementYaw = $Index
                sub(/^visual_to_movement_yaw=/, "", VisualMovementYaw)
            }
        }
        # A combat target deliberately owns the visible mesh yaw while the
        # capsule/legs follow a route. Only patrol/investigation movement
        # without a target must remain visually travel-facing.
        TargetFacing = 0
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "target_facing=yes") {
                TargetFacing = 1
            }
        }
        if (MovementActive && !TargetFacing && VisualMovementYaw != "" && ((VisualMovementYaw + 0) > 80 || (VisualMovementYaw + 0) < -80)) {
            Count++
        }
    }
    END { print Count + 0 }
' "${STDOUT_PATH}")"
BodyTargetFacingMismatchCount="$(awk '
    {
        TargetFacing = 0
        MovementActive = 0
        BodyTargetYaw = ""
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "target_facing=yes") {
                TargetFacing = 1
            } else if ($Index == "movement_active=yes") {
                MovementActive = 1
            } else if ($Index ~ /^body_to_target_yaw=/) {
                BodyTargetYaw = $Index
                sub(/^body_to_target_yaw=/, "", BodyTargetYaw)
            }
        }
        if (TargetFacing && !MovementActive && BodyTargetYaw != "" && ((BodyTargetYaw + 0) > 45 || (BodyTargetYaw + 0) < -45)) {
            Count++
        }
    }
    END { print Count + 0 }
' "${STDOUT_PATH}")"
CombatMovementFacingMismatchCount="$(awk '
    {
        TargetFacing = 0
        MovementActive = 0
        BodyMovementYaw = ""
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "target_facing=yes") {
                TargetFacing = 1
            } else if ($Index == "movement_active=yes") {
                MovementActive = 1
            } else if ($Index ~ /^body_to_movement_yaw=/) {
                BodyMovementYaw = $Index
                sub(/^body_to_movement_yaw=/, "", BodyMovementYaw)
            }
        }
        # Combat locomotion has no strafe/backward rifle clip. While a hostile
        # target owns the body heading, the actual travel route must remain in
        # the same readable 45-degree cone or the soldier will slide sideways.
        if (TargetFacing && MovementActive && BodyMovementYaw != "" && ((BodyMovementYaw + 0) > 45 || (BodyMovementYaw + 0) < -45)) {
            Count++
        }
    }
    END { print Count + 0 }
' "${STDOUT_PATH}")"
CombatMovementFacingSampleCount="$(awk '
    {
        TargetFacing = 0
        MovementActive = 0
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index == "target_facing=yes") {
                TargetFacing = 1
            } else if ($Index == "movement_active=yes") {
                MovementActive = 1
            }
        }
        if (TargetFacing && MovementActive) {
            Count++
        }
    }
    END { print Count + 0 }
' "${STDOUT_PATH}")"
MovementDisplacementSummary="$(awk '
    /Black Meridian showcase: enemy index=/ {
        Pawn = ""
        X = 0
        Y = 0
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index ~ /^pawn=/) {
                Pawn = $Index
                sub(/^pawn=/, "", Pawn)
            } else if ($Index ~ /^location=X=/) {
                X = $Index
                sub(/^location=X=/, "", X)
            } else if ($Index ~ /^Y=/) {
                Y = $Index
                sub(/^Y=/, "", Y)
            }
        }
        if (Pawn != "") {
            SpawnX[Pawn] = X
            SpawnY[Pawn] = Y
            Spawned[Pawn] = 1
        }
        next
    }
    /Black Meridian AI movement: pawn=/ {
        Pawn = ""
        X = 0
        Y = 0
        for (Index = 1; Index <= NF; ++Index) {
            if ($Index ~ /^pawn=/) {
                Pawn = $Index
                sub(/^pawn=/, "", Pawn)
            } else if ($Index ~ /^location=X=/) {
                X = $Index
                sub(/^location=X=/, "", X)
            } else if ($Index ~ /^Y=/) {
                Y = $Index
                sub(/^Y=/, "", Y)
            }
        }
        if (Pawn == "") {
            next
        }
        if (!(Pawn in Spawned)) {
            next
        }
        Distance = sqrt((X - SpawnX[Pawn]) * (X - SpawnX[Pawn]) + (Y - SpawnY[Pawn]) * (Y - SpawnY[Pawn]))
        if (Distance > MaxDistance[Pawn]) {
            MaxDistance[Pawn] = Distance
        }
    }
    END {
        Meaningful = 0
        Maximum = 0
        for (Pawn in MaxDistance) {
            if (MaxDistance[Pawn] >= 120.0) {
                Meaningful++
            }
            if (MaxDistance[Pawn] > Maximum) {
                Maximum = MaxDistance[Pawn]
            }
        }
        printf "%d %.1f", Meaningful, Maximum
    }
' "${STDOUT_PATH}")"
MeaningfulMovementCount="${MovementDisplacementSummary%% *}"
MaxMovementDisplacementCm="${MovementDisplacementSummary##* }"

if (( EnemyCount < 5 || MovementCount < 5 || MeaningfulMovementCount < 5 || FireCount < 1 || RoleCount < 5 || DemolitionistRuntimeMatch != 1 || FacingSampleCount < 5 || FacingMismatchCount > 0 || ForwardMovementMismatchCount > 0 || VisualMovementMismatchCount > 0 || TargetFacingSampleCount < 5 || TargetFacingMismatchCount > 0 || BodyTargetFacingMismatchCount > 0 || CombatMovementFacingMismatchCount > 0 )); then
    Result="Black Meridian combat role probe: FAIL enemies=${EnemyCount} roles=${RoleCount} demolitionist_runtime=${DemolitionistRuntimeMatch} movement_events=${MovementCount} meaningful_movement_pawns=${MeaningfulMovementCount} max_displacement_cm=${MaxMovementDisplacementCm} facing_samples=${FacingSampleCount} facing_mismatches=${FacingMismatchCount} forward_movement_mismatches=${ForwardMovementMismatchCount} visual_movement_mismatches=${VisualMovementMismatchCount} body_target_facing_mismatches=${BodyTargetFacingMismatchCount} combat_route_facing_samples=${CombatMovementFacingSampleCount} combat_route_facing_mismatches=${CombatMovementFacingMismatchCount} visual_target_facing_samples=${TargetFacingSampleCount} visual_target_facing_mismatches=${TargetFacingMismatchCount} role_fire_events=${FireCount} tolerance_deg=45 forward_tolerance_deg=80"
    printf '%s\n' "${Result}" | tee "${RESULT_PATH}" >&2
    exit 1
fi

Result="Black Meridian combat role probe: PASS enemies=${EnemyCount} roles=${RoleCount} demolitionist_runtime=${DemolitionistRuntimeMatch} movement_events=${MovementCount} meaningful_movement_pawns=${MeaningfulMovementCount} max_displacement_cm=${MaxMovementDisplacementCm} facing_samples=${FacingSampleCount} facing_mismatches=${FacingMismatchCount} forward_movement_mismatches=${ForwardMovementMismatchCount} visual_movement_mismatches=${VisualMovementMismatchCount} body_target_facing_mismatches=${BodyTargetFacingMismatchCount} combat_route_facing_samples=${CombatMovementFacingSampleCount} combat_route_facing_mismatches=${CombatMovementFacingMismatchCount} visual_target_facing_samples=${TargetFacingSampleCount} visual_target_facing_mismatches=${TargetFacingMismatchCount} role_fire_events=${FireCount} tolerance_deg=45 forward_tolerance_deg=80"
printf '%s\n' "${Result}" | tee "${RESULT_PATH}"
