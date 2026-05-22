#!/usr/bin/env bash
# Run all RPAL test cases and compare rpal20 output against rpal.exe (via Wine).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RPAL20="$SCRIPT_DIR/rpal20"
RPAL_REF="$SCRIPT_DIR/rpal/rpal.exe"
TEST_DIR="$SCRIPT_DIR/rpal"

# ── colours ──────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
RESET='\033[0m'

pass() { printf "${GREEN}PASS${RESET}  %s\n" "$1"; }
fail() { printf "${RED}FAIL${RESET}  %s\n" "$1"; }
warn() { printf "${YELLOW}WARN${RESET}  %s\n" "$1"; }

# ── preflight checks ─────────────────────────────────────────────────────────
if [[ ! -x "$RPAL20" ]]; then
    printf "${BOLD}rpal20 not found — building...${RESET}\n"
    make -C "$SCRIPT_DIR" -s
fi

if [[ ! -f "$RPAL_REF" ]]; then
    printf "${RED}error:${RESET} reference binary not found at %s\n" "$RPAL_REF" >&2
    exit 1
fi

if ! command -v wine &>/dev/null; then
    printf "${RED}error:${RESET} wine is required to run rpal.exe\n" >&2
    exit 1
fi

# ── collect test files ───────────────────────────────────────────────────────
# Test files: everything in rpal/ that is a regular file and not a binary/dll
mapfile -t TESTS < <(
    find "$TEST_DIR" -maxdepth 1 -type f \
        ! -name "*.exe" ! -name "*.dll" ! -name "*.stackdump" \
        | sort
)

if [[ ${#TESTS[@]} -eq 0 ]]; then
    printf "${YELLOW}No test files found in %s${RESET}\n" "$TEST_DIR"
    exit 0
fi

# ── run tests ────────────────────────────────────────────────────────────────
PASS=0
FAIL=0
ERRORS=()

printf "\n${BOLD}Running %d test case(s)...${RESET}\n\n" "${#TESTS[@]}"

for test_file in "${TESTS[@]}"; do
    name="$(basename "$test_file")"
    tmp_ours="$(mktemp)"
    tmp_ref="$(mktemp)"

    # Run our interpreter (capture stderr separately so crashes don't pollute diff)
    if ! "$RPAL20" "$test_file" > "$tmp_ours" 2>&1; then
        our_exit=$?
    else
        our_exit=0
    fi

    # Run reference interpreter via Wine (suppress Wine debug noise; strip Windows \r)
    WINEDEBUG=-all wine "$RPAL_REF" "$test_file" 2>/dev/null | tr -d '\r' > "$tmp_ref" || true

    if diff -q "$tmp_ours" "$tmp_ref" &>/dev/null; then
        pass "$name"
        PASS=$((PASS + 1))
    else
        fail "$name"
        FAIL=$((FAIL + 1))
        ERRORS+=("$name")
        # Show a compact diff
        printf "  ${BOLD}Expected (ref):${RESET}\n"
        sed 's/^/    /' "$tmp_ref"
        printf "  ${BOLD}Got (ours):${RESET}\n"
        sed 's/^/    /' "$tmp_ours"
        echo
    fi

    rm -f "$tmp_ours" "$tmp_ref"
done

# ── summary ──────────────────────────────────────────────────────────────────
TOTAL=$((PASS + FAIL))
printf "\n${BOLD}Results: %d/%d passed${RESET}" "$PASS" "$TOTAL"

if [[ $FAIL -gt 0 ]]; then
    printf "  (${RED}%d failed${RESET}: %s)" "$FAIL" "${ERRORS[*]}"
fi
printf "\n\n"

[[ $FAIL -eq 0 ]]
