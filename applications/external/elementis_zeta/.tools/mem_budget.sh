#!/usr/bin/env bash
# Memory-budget guard (contract section 5.x): builds the app and its four plugins in BOTH modes, then checks the budget.
#   RELEASE build (default `ufbt build`, no NUCLEO_DEBUG) = the gate: ratchets, targets, floors, baselines (it is what the player runs).
#   DEV build (`NUCLEO_DEBUG=1 ufbt build`, debug overlay) = only checked against the same heap floors (the maintainer's device tests run it).
#   bash tools/mem_budget.sh                  build both + check (exit 1 = a budget was broken)
#   bash tools/mem_budget.sh --update         ... and lower the ratchets / raise the baselines of the release build when things shrank
#   bash tools/mem_budget.sh --raise battle 32500 --reason "why"     (see tools/mem_budget.py --help)
#   bash tools/mem_budget.sh --rebase --reason "why"      re-base every release ratchet / baseline on this build (logged)
#   NO_BUILD=1 bash tools/mem_budget.sh       skip `ufbt build`: use the ELFs of the last run in build/mem_budget/elf_release and elf_dev
#                                             (when elf_release is missing, the ELFs in ~/.ufbt/build are taken as the release build)
#   NO_DEV=1 bash tools/mem_budget.sh         release only (quick loop; prints a WARN that the dev build was not checked)
#   bash tools/mem_budget.sh --selftest       only run the tool's own host test
# Every task's "Done when" includes this script passing (WARN is fine, FAIL is not).
# The modes are built dev first and release last, so ~/.ufbt/build ends with the release build (the default `ufbt launch`).
# Before each build the old nucleo objects and ELFs are removed (rm -rf $UFBT_HOME/build/elementis_zeta* ez_*: other apps share that directory and stay).
set -e
cd "$(dirname "$0")/.."
# the project venv (`make setup`); without it (CI, a system python with requirements.txt installed) the python3 in PATH is used
if [ -f .venv/bin/activate ]; then source .venv/bin/activate; fi
if [ "$1" = "--selftest" ]; then
    exec python3 -I tools/mem_budget_test.py
fi
UHOME="${UFBT_HOME:-$HOME/.ufbt}"
SNAP=build/mem_budget
rel_dir="$SNAP/elf_release"
dev_dir="$SNAP/elf_dev"

build_mode() {   # build_mode <release|dev> <snapshot dir>
    local mode="$1" dir="$2" log
    echo "== ufbt build ($mode)"
    rm -rf "$UHOME"/build/elementis_zeta* "$UHOME"/build/ez_* "$UHOME"/build/nucleo*
    log=$(mktemp)
    if [ "$mode" = dev ]; then export NUCLEO_DEBUG=1; else unset NUCLEO_DEBUG; fi
    if ! (cd nucleo && ufbt build) >"$log" 2>&1; then
        tail -30 "$log"; rm -f "$log"
        echo "mem_budget: ufbt build ($mode) FAILED"; exit 2
    fi
    rm -f "$log"
    unset NUCLEO_DEBUG
    rm -rf "$dir"; mkdir -p "$dir"
    cp "$UHOME"/build/elementis_zeta_d.elf "$UHOME"/build/ez_*_d.elf "$UHOME"/build/ez_*.fal "$dir"/
}

args=("$@")
case "$1" in
    --raise|--raise-baseline) ;;   # these only edit tools/mem_budget.toml: no build needed
    *)
        if [ -z "$NO_BUILD" ]; then
            [ -n "$NO_DEV" ] || build_mode dev "$dev_dir"
            build_mode release "$rel_dir"
        elif [ ! -d "$rel_dir" ]; then
            mkdir -p "$rel_dir"; cp "$UHOME"/build/elementis_zeta_d.elf "$UHOME"/build/ez_*_d.elf "$UHOME"/build/ez_*.fal "$rel_dir"/
        fi
        args=(--elf-dir "$rel_dir")
        if [ -z "$NO_DEV" ] && [ -d "$dev_dir" ]; then args+=(--dev-elf-dir "$dev_dir"); fi
        args+=("$@")
        ;;
esac
exec python3 tools/mem_budget.py "${args[@]}"
