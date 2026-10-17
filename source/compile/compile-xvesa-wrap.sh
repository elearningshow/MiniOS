#!/bin/bash
set -euo pipefail
POC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore
gcc -m32 -O2 -Wall -o "$POC/scripts/minios-xvesa-wrap" "$POC/scripts/minios-xvesa-wrap.c"
file "$POC/scripts/minios-xvesa-wrap"
echo COMPILE_XVESA_WRAP_OK
