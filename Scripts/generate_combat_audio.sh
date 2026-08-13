#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-${PROJECT_ROOT}/Content/Audio/Generated}"
FFMPEG_BIN="${FFMPEG_BIN:-$(command -v ffmpeg || true)}"

if [[ -z "${FFMPEG_BIN}" || ! -x "${FFMPEG_BIN}" ]]; then
    echo "ffmpeg is required to generate the project-owned combat cues" >&2
    exit 2
fi

mkdir -p "${OUTPUT_DIR}"

render_mono() {
    local name="$1"
    local duration="$2"
    local source="$3"
    local filters="$4"
    "${FFMPEG_BIN}" -hide_banner -loglevel error -y \
        -f lavfi -i "${source}" \
        -t "${duration}" -af "${filters}" \
        -ar 48000 -ac 1 -c:a pcm_s16le "${OUTPUT_DIR}/${name}.wav"
}

render_stereo() {
    local name="$1"
    local duration="$2"
    local source="$3"
    local filters="$4"
    "${FFMPEG_BIN}" -hide_banner -loglevel error -y \
        -f lavfi -i "${source}" \
        -t "${duration}" -af "${filters}" \
        -ar 48000 -ac 2 -c:a pcm_s16le "${OUTPUT_DIR}/${name}.wav"
}

# These cues are deliberately synthesized from deterministic signal/noise
# layers so the project owns the source and can regenerate the same set on any
# build host. They are short, dry source cues intended for UE distance/mix
# processing, not final master recordings.
render_mono BM_ARX4_Fire 0.24 \
    "anoisesrc=color=white:amplitude=0.55:duration=0.24" \
    "highpass=f=65,lowpass=f=9000,afade=t=out:st=0.015:d=0.225,volume=0.92"
render_mono BM_Suppressed_Fire 0.18 \
    "anoisesrc=color=white:amplitude=0.36:duration=0.18" \
    "highpass=f=180,lowpass=f=5200,afade=t=out:st=0.01:d=0.17,volume=0.68"
render_mono BM_Impact_Concrete 0.16 \
    "anoisesrc=color=brown:amplitude=0.48:duration=0.16" \
    "lowpass=f=3100,highpass=f=90,afade=t=out:st=0.005:d=0.15,volume=0.74"
render_mono BM_Impact_Metal 0.24 \
    "sine=frequency=1460:duration=0.24" \
    "afade=t=out:st=0.01:d=0.23,highpass=f=420,volume=0.38"
render_mono BM_Reload 0.42 \
    "anoisesrc=color=white:amplitude=0.16:duration=0.42" \
    "highpass=f=420,lowpass=f=6500,afade=t=out:st=0.02:d=0.38,volume=0.38"
render_mono BM_DryFire 0.10 \
    "sine=frequency=185:duration=0.10" \
    "afade=t=out:st=0.005:d=0.09,volume=0.58"
render_mono BM_Footstep_Concrete 0.13 \
    "anoisesrc=color=pink:amplitude=0.35:duration=0.13" \
    "highpass=f=120,lowpass=f=1900,afade=t=out:st=0.015:d=0.11,volume=0.40"
render_mono BM_Footstep_Metal 0.18 \
    "sine=frequency=820:duration=0.18" \
    "afade=t=out:st=0.01:d=0.17,highpass=f=300,volume=0.27"
render_mono BM_Footstep_SoftCover 0.14 \
    "anoisesrc=color=brown:amplitude=0.24:duration=0.14" \
    "lowpass=f=900,afade=t=out:st=0.01:d=0.12,volume=0.30"
render_mono BM_Explosion 1.10 \
    "anoisesrc=color=brown:amplitude=0.82:duration=1.10" \
    "lowpass=f=1800,highpass=f=36,afade=t=out:st=0.04:d=1.05,volume=0.86"
render_mono BM_Hit_Confirm 0.12 \
    "sine=frequency=1180:duration=0.12" \
    "afade=t=out:st=0.005:d=0.11,volume=0.47"
render_mono BM_Elimination 0.32 \
    "sine=frequency=680:duration=0.32" \
    "afade=t=out:st=0.015:d=0.30,highpass=f=170,volume=0.42"
render_mono BM_Suppression 0.26 \
    "sine=frequency=64:duration=0.26" \
    "afade=t=out:st=0.015:d=0.23,lowpass=f=520,volume=0.34"
render_mono BM_LowHealth 0.28 \
    "sine=frequency=112:duration=0.28" \
    "afade=t=out:st=0.01:d=0.26,highpass=f=70,volume=0.38"
render_mono BM_ThrowableWarning 0.14 \
    "sine=frequency=720:duration=0.14" \
    "afade=t=out:st=0.006:d=0.12,highpass=f=260,volume=0.40"

# Short loop-safe beds. Runtime music remains the fallback if these are not
# assigned, but having real PCM beds lets the same phase router be mixed and
# auditioned in the cooked build.
render_stereo BM_Music_Mission 8.0 \
    "sine=frequency=110:duration=8" \
    "afade=t=out:st=7.2:d=0.8,volume=0.12"
render_stereo BM_Music_Suspense 8.0 \
    "sine=frequency=62:duration=8" \
    "afade=t=out:st=7.2:d=0.8,volume=0.16"
render_stereo BM_Music_Combat 8.0 \
    "sine=frequency=92:duration=8" \
    "afade=t=out:st=7.2:d=0.8,volume=0.15"
render_stereo BM_Music_Complete 4.0 \
    "sine=frequency=220:duration=4" \
    "afade=t=out:st=3.3:d=0.7,volume=0.14"

echo "Generated $(find "${OUTPUT_DIR}" -maxdepth 1 -name '*.wav' | wc -l | tr -d ' ') project-owned combat cues in ${OUTPUT_DIR}"
