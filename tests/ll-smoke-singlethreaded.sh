!/bin/bash

# Singlethreaded ll-smoke test.
#
# When multithreading is enabled at build time, it is the default
# behavior of squashfuse_ll, but can be disabled at runtime with
# the FUSE '-s' commandline option.
#
# So we just re-run the normal ll-smoke test with the '-s' option.
SFLL_EXTRA_ARGS="-s" $(dirname -- $0)/ll-smoke.sh
