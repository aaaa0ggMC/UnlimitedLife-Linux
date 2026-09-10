#!/usr/bin/env bash
source "$(dirname -- "${BASH_SOURCE[0]}")/common.sh"
if [[ "${MSYSTEM:-}" != UCRT64 ]]; then
    printf 'Run this command in an MSYS2 UCRT64 terminal.\n' >&2
    exit 1
fi
exec bash "$PROJECT_ROOT/scripts/configure" -p mingw "$@"
