#!/bin/sh
# Builds and runs the table summary tests (they need no SDL, only a C compiler)
set -e
cd "$(dirname "$0")/.."
mkdir -p build
cc -O2 -Wall -o build/summary_test tests/summary_test.c src/gsummary.c -lm
build/summary_test
cc -O2 -Wall -o build/wsummary_test tests/wsummary_test.c src/gwsummary.c src/gsummary.c -lm
build/wsummary_test
cc -O2 -Wall -o build/cmdsummary_test tests/cmdsummary_test.c src/gcmdsummary.c src/gwsummary.c src/gsummary.c -lm
build/cmdsummary_test
cc -O2 -Wall -o build/stepedit_test tests/stepedit_test.c src/gstepedit.c src/gwsummary.c src/gfsummary.c src/gsummary.c -lm
build/stepedit_test
cc -O2 -Wall -o build/fsummary_test tests/fsummary_test.c src/gfsummary.c src/gsummary.c -lm
build/fsummary_test
cc -O2 -Wall -fsanitize=address,undefined -o build/midiparse_test tests/midiparse_test.c src/gmidiparse.c
build/midiparse_test
cc -O2 -Wall -fsanitize=address,undefined -o build/keylayout_test tests/keylayout_test.c src/gkeylayout.c
build/keylayout_test
cc -O2 -Wall -fsanitize=address,undefined -o build/voices_test tests/voices_test.c src/gvoices.c
build/voices_test
cc -O2 -Wall -fsanitize=address,undefined -o build/insuse_test tests/insuse_test.c src/ginsuse.c
build/insuse_test
cc -O2 -Wall -fsanitize=address,undefined -o build/ordhint_test tests/ordhint_test.c src/gordhint.c
build/ordhint_test
cc -O2 -Wall -fsanitize=address,undefined -o build/scope_test tests/scope_test.c src/gscope.c -lm
build/scope_test
