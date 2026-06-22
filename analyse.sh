#!/usr/bin/env bash

set -euo pipefail

mkdir -p build-release results

rm results/* || true

NPROC=$(nproc)

INPUT=examples/arbiter.json
IDEAL=examples/arbiter-fixed.json

cmake --build --parallel "$NPROC" --preset release

./build-release/counter \
    --input "$INPUT" \
    --output-dir results

./build-release/compare \
    --repairs results \
    --ideal "$IDEAL"
