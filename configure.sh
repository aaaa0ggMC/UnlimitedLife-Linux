#!/usr/bin/env bash
# Compatibility entry point; dependencies are managed by xmake.
set -euo pipefail
exec bash "$(dirname -- "${BASH_SOURCE[0]}")/scripts/configure" "$@"
