#!/bin/sh
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export PATH="$DIR/.deps/usr/bin:$PATH"
export LD_LIBRARY_PATH="$DIR/.deps/usr/lib64:$DIR/.deps/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_QPA_PLATFORM=offscreen
"$DIR/build/latex-pad-tests" "$@"
