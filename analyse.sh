#!/usr/bin/env bash

set -euo pipefail

mkdir -p build-release results

rm -f results/*

NPROC=$(nproc)

INPUT=examples/takeoff.json
IDEALS=(
    examples/takeoff-fixed-1.json
    examples/takeoff-fixed-2.json
)

cmake --build --parallel "$NPROC" --preset release

./build-release/counter \
    --input "$INPUT" \
    --output-dir results \
    --config example-config.toml

IDEAL_ARGS=()
for ideal in "${IDEALS[@]}"; do
    IDEAL_ARGS+=(--ideal "$ideal")
done

./build-release/compare \
    --repairs results \
    "${IDEAL_ARGS[@]}"
