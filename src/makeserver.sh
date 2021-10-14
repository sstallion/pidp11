
#!/bin/bash

# stop on error
set -e

# Debugging:
# set -x

pwd
export MAKETARGETS="pidp1170_blinkenlightd"

#export MAKE_CONFIGURATION=DEBUG
export MAKE_CONFIGURATION=RELEASE

(
# the Blinkenlight API server for PiDP11
cd 11_pidp_server/pidp11
echo ; echo "*** blinkenlight_server for PiDP11"
MAKE_TARGET_ARCH=RPI make $MAKEOPTIONS $MAKETARGETS
)

echo
echo "Server compiled OK!"
