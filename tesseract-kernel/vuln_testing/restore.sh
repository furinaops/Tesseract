#!/bin/bash
#
# restore.sh — Restore the production base_kernel (kernel_template)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== Restoring production kernel_template ==="

make -C "$PROJ_DIR" clean 2>/dev/null
# Rebuild from scratch — base_kernel.img from kernel_template
make -C "$PROJ_DIR" base_kernel 2>/dev/null
make -C "$PROJ_DIR" hyperkernel 2>/dev/null
make -C "$PROJ_DIR" iso 2>/dev/null

echo "=== Production kernel restored ==="
