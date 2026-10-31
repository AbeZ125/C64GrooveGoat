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
cc -O2 -Wall -o build/fsummary_test tests/fsummary_test.c src/gfsummary.c src/gsummary.c -lm
build/fsummary_test
