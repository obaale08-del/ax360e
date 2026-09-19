#!/bin/bash
CORE_PATH="$(dirname "$0")/../core/video_core.sh"

if [ -f "$CORE_PATH" ]; then
    source "$CORE_PATH"
else
    echo "[ERROR] El núcleo blindado no está disponible."
    exit 1
fi

execute_pipeline() {
    local target_input="$1"
    local target_output="$2"

    if [ -z "$target_input" ] || [ -z "$target_output" ]; then
        echo "Uso: $0 <archivo_entrada> <directorio_salida>"
        exit 1
    fi

    mkdir -p "$target_output"
    run_core_video "$target_input" "$target_output"
}

execute_pipeline "$1" "$2"
