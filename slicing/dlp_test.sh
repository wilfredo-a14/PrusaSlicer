#!/usr/bin/env bash
# Headless DLP / corkscrew test runner for PrusaSlicer.
#
# Runs slicing without opening the GUI. Useful for iterating on corkscrew
# partitioning and PNG layer export from the terminal.
#
# Usage:
#   ./slicing/dlp_test.sh model.stl
#   ./slicing/dlp_test.sh model.stl --corkscrew-box-count 8
#   ./slicing/dlp_test.sh model.stl --no-corkscrew-enable
#
# Environment overrides:
#   PRUSASLICER_BIN   path to PrusaSlicer executable
#   DLP_OUTPUT_DIR    directory for .sl1 output (default: ./output)
#   DLP_PNG_DIR       directory for PNG layers (default: ./output/png)
#   DLP_PRINT_PROFILE print preset name
#   DLP_MATERIAL_PROFILE material preset name
#   DLP_PRINTER_PROFILE printer preset name

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

PRUSASLICER_BIN="${PRUSASLICER_BIN:-${ROOT_DIR}/build/src/PrusaSlicer}"
DLP_OUTPUT_DIR="${DLP_OUTPUT_DIR:-${ROOT_DIR}/output}"
DLP_PNG_DIR="${DLP_PNG_DIR:-${DLP_OUTPUT_DIR}/png}"

DLP_PRINT_PROFILE="${DLP_PRINT_PROFILE:-0.05mm Normal @SL1S}"
DLP_MATERIAL_PROFILE="${DLP_MATERIAL_PROFILE:-Prusament Resin Tough Prusa Orange @SL1S}"
DLP_PRINTER_PROFILE="${DLP_PRINTER_PROFILE:-Original Prusa SL1S @SL1S}"

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 model.stl [extra prusaslicer args...]" >&2
    exit 2
fi

MODEL="$1"
shift

if [[ ! -x "${PRUSASLICER_BIN}" ]]; then
    echo "PrusaSlicer binary not found or not executable: ${PRUSASLICER_BIN}" >&2
    echo "Build first or set PRUSASLICER_BIN." >&2
    exit 1
fi

if [[ ! -f "${MODEL}" ]]; then
    echo "Model file not found: ${MODEL}" >&2
    exit 1
fi

mkdir -p "${DLP_OUTPUT_DIR}" "${DLP_PNG_DIR}"

MODEL_BASENAME="$(basename "${MODEL}")"
MODEL_STEM="${MODEL_BASENAME%.*}"
SL1_OUT="${DLP_OUTPUT_DIR}/${MODEL_STEM}.sl1"

echo "==> DLP headless slice"
echo "    binary:  ${PRUSASLICER_BIN}"
echo "    model:   ${MODEL}"
echo "    output:  ${SL1_OUT}"
echo "    png dir: ${DLP_PNG_DIR}"
echo "    log:     logs/dlp_corkscrew.log (under cwd)"
echo

cd "${ROOT_DIR}"

SLICER_ARGS=(
    --export-sla
    --print-profile "${DLP_PRINT_PROFILE}"
    --material-profile "${DLP_MATERIAL_PROFILE}"
    --printer-profile "${DLP_PRINTER_PROFILE}"
    --corkscrew-enable
    --export-png-dir "${DLP_PNG_DIR}"
    --output "${SL1_OUT}"
)

# Use installed presets when available (optional).
if [[ -n "${DLP_DATADIR:-}" ]]; then
    SLICER_ARGS+=(--datadir "${DLP_DATADIR}")
elif [[ -d "${HOME}/Library/Application Support/PrusaSlicer" ]]; then
    SLICER_ARGS+=(--datadir "${HOME}/Library/Application Support/PrusaSlicer")
elif [[ -d "${HOME}/.config/PrusaSlicer" ]]; then
    SLICER_ARGS+=(--datadir "${HOME}/.config/PrusaSlicer")
fi

set +e
"${PRUSASLICER_BIN}" \
    "${SLICER_ARGS[@]}" \
    "$@" \
    "${MODEL}"
EXIT=$?
set -e

if [[ ${EXIT} -eq 0 ]]; then
    echo
    echo "==> Slice succeeded"
    echo "    SL1:  ${SL1_OUT}"
    echo "    PNGs: ${DLP_PNG_DIR}"
    if [[ -f logs/dlp_corkscrew.log ]]; then
        echo "    Log:  ${ROOT_DIR}/logs/dlp_corkscrew.log"
        echo
        echo "--- last corkscrew log lines ---"
        tail -n 8 logs/dlp_corkscrew.log
    fi
else
    echo
    echo "==> Slice failed (exit ${EXIT})" >&2
    if [[ -f logs/dlp_corkscrew.log ]]; then
        echo "--- last corkscrew log lines ---" >&2
        tail -n 12 logs/dlp_corkscrew.log >&2
    fi
fi

exit ${EXIT}
