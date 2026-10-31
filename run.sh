#!/bin/sh
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)
if [ -d "$DIR/.deps/usr" ]; then
    PATH="$DIR/.deps/usr/bin:$PATH"
    LD_LIBRARY_PATH="$DIR/.deps/usr/lib64:$DIR/.deps/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export PATH LD_LIBRARY_PATH
fi
exec "$DIR/build/latex-pad" "$@"
