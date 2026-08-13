#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-${PROJECT_ROOT}/Content/Audio/Generated}"
FFMPEG_BIN="${FFMPEG_BIN:-$(command -v ffmpeg || true)}"

if [[ -z "${FFMPEG_BIN}" || ! -x "${FFMPEG_BIN}" ]]; then
    echo "ffmpeg is required to generate the project-owned environment beds" >&2
    exit 2
fi

mkdir -p "${OUTPUT_DIR}"

render_bed() {
    local name="$1"
    local source="$2"
    local filters="$3"
    "${FFMPEG_BIN}" -hide_banner -loglevel error -y \
        -f lavfi -i "${source}" \
        -t 8.0 -af "${filters},afade=t=out:st=7.2:d=0.8" \
        -ar 48000 -ac 2 -c:a pcm_s16le "${OUTPUT_DIR}/${name}.wav"
}

# These are deliberately restrained, loop-safe source beds. They are distinct
# environmental fingerprints for the twenty operations, not final music:
# water, storm, machinery, forest air, desert wind, ice, and interior hum all
# occupy different frequency bands so the theme remains audible underneath
# the phase music without competing with combat cues.
render_bed BM_Ambient_SableQuay "anoisesrc=color=pink:amplitude=0.10:duration=8" "lowpass=f=230,highpass=f=42,volume=0.28"
render_bed BM_Ambient_Floodline "anoisesrc=color=white:amplitude=0.16:duration=8" "highpass=f=650,lowpass=f=8200,volume=0.22"
render_bed BM_Ambient_RelayHouse "sine=frequency=78:duration=8" "tremolo=f=0.32:d=0.68,lowpass=f=620,volume=0.16"
render_bed BM_Ambient_NightglassCore "sine=frequency=44:duration=8" "tremolo=f=0.18:d=0.85,lowpass=f=360,volume=0.24"
render_bed BM_Ambient_AshenDocks "anoisesrc=color=brown:amplitude=0.16:duration=8" "lowpass=f=1450,highpass=f=80,volume=0.20"
render_bed BM_Ambient_CinderWorks "sine=frequency=112:duration=8" "tremolo=f=1.6:d=0.72,lowpass=f=520,volume=0.20"
render_bed BM_Ambient_SaltFoundry "anoisesrc=color=pink:amplitude=0.12:duration=8" "lowpass=f=540,highpass=f=70,volume=0.18"
render_bed BM_Ambient_BlackwaterYard "anoisesrc=color=brown:amplitude=0.14:duration=8" "bandpass=f=280:width_type=h:width=1.3,volume=0.22"
render_bed BM_Ambient_GlassMarket "sine=frequency=430:duration=8" "vibrato=f=0.7:d=0.08,highpass=f=180,lowpass=f=1900,volume=0.10"
render_bed BM_Ambient_StormVault "anoisesrc=color=white:amplitude=0.18:duration=8" "highpass=f=180,lowpass=f=4200,tremolo=f=0.24:d=0.78,volume=0.22"
render_bed BM_Ambient_RedTide "anoisesrc=color=brown:amplitude=0.13:duration=8" "lowpass=f=470,highpass=f=55,volume=0.22"
render_bed BM_Ambient_IronCathedral "sine=frequency=58:duration=8" "tremolo=f=0.42:d=0.62,lowpass=f=940,volume=0.18"
render_bed BM_Ambient_LanternBasin "anoisesrc=color=pink:amplitude=0.11:duration=8" "bandpass=f=2400:width_type=h:width=1.5,volume=0.13"
render_bed BM_Ambient_QuarryNine "anoisesrc=color=brown:amplitude=0.15:duration=8" "lowpass=f=360,highpass=f=48,tremolo=f=0.16:d=0.82,volume=0.22"
render_bed BM_Ambient_VantaSpire "anoisesrc=color=white:amplitude=0.12:duration=8" "bandpass=f=920:width_type=h:width=1.2,volume=0.17"
render_bed BM_Ambient_Wreckline "sine=frequency=190:duration=8" "tremolo=f=0.28:d=0.70,lowpass=f=1100,volume=0.14"
render_bed BM_Ambient_EmberStation "sine=frequency=132:duration=8" "tremolo=f=0.72:d=0.76,lowpass=f=720,volume=0.18"
render_bed BM_Ambient_PaleHarbor "anoisesrc=color=pink:amplitude=0.14:duration=8" "lowpass=f=720,highpass=f=120,tremolo=f=0.20:d=0.80,volume=0.20"
render_bed BM_Ambient_MeridianGate "sine=frequency=96:duration=8" "tremolo=f=0.55:d=0.62,lowpass=f=640,volume=0.16"
render_bed BM_Ambient_LastLight "sine=frequency=148:duration=8" "tremolo=f=0.22:d=0.72,lowpass=f=860,volume=0.14"

echo "Generated $(find "${OUTPUT_DIR}" -maxdepth 1 -name 'BM_Ambient_*.wav' | wc -l | tr -d ' ') environment beds in ${OUTPUT_DIR}"
