
#!/bin/bash


# stop on error
set -e

# Debugging:
# set -x


# needed packages:



# compile all binaries for all platforms
pwd
export MAKEOPTIONS=--silent
export MAKETARGETS="clean all"

# optimize all compiles, see makefiles
#export MAKE_CONFIGURATION=DEBUG
export MAKE_CONFIGURATION=RELEASE


(
# All classes and resources for all Java panels into one jar
#sudo apt-get install ant openjdk-7-jdk
cd 09_javapanelsim
ant -f build.xml compile jar
)


(
# the Blinkenligt API server for BlinkenBus is only useful on BEAGLEBONE
# Simulation and syntax test modes work also on desktop Linuxes
cd 07.1_blinkenlight_server
echo ; echo "*** blinkenlight_server for BeagleBoneWhite"
MAKE_TARGET_ARCH=BBW make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** blinkenlight_server for BeagleBoneBlack"
MAKE_TARGET_ARCH=BBB make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** blinkenlight_server for x86"
MAKE_TARGET_ARCH=X86 make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** blinkenlight_server for x64"
MAKE_TARGET_ARCH=X64 make $MAKEOPTIONS $MAKETARGETS
)

(
# the Blinkenligt API server for BlinkenBoard PDP-15
cd 07.3_blinkenlight_server_pdp15
echo ; echo "*** blinkenlight_server for PDP-15 on BeagleBoneWhite"
MAKE_TARGET_ARCH=BBW make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** blinkenlight_server for PDP-15 on BeagleBoneBlack"
MAKE_TARGET_ARCH=BBB make $MAKEOPTIONS $MAKETARGETS
)


(
# the Blinkenligt API server for Oscar Vermeulen's PiDP8
cd 11_pidp_server/pidp8
echo ; echo "*** blinkenlight_server for PiDP8"
MAKE_TARGET_ARCH=RPI make $MAKEOPTIONS $MAKETARGETS
)
(
# the Blinkenligt API server for Oscar Vermeulen's PiDP11
cd 11_pidp_server/pidp11
echo ; echo "*** blinkenlight_server for PiDP11"
MAKE_TARGET_ARCH=RPI make $MAKEOPTIONS $MAKETARGETS
)

(
# The Blinkenligt API test client for all platforms
cd 07.2_blinkenlight_test
echo ; echo "*** blinkenlight_test for x86"
MAKE_TARGET_ARCH=X86 make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** blinkenlight_test for x64"
MAKE_TARGET_ARCH=X64 make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** blinkenlight_test for RaspberryPi"
MAKE_TARGET_ARCH=RPI make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** blinkenlight_test for BeagleBoneWhite"
MAKE_TARGET_ARCH=BBW make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** blinkenlight_test for BeagleBoneBlack"
MAKE_TARGET_ARCH=BBB make $MAKEOPTIONS $MAKETARGETS
)


(
# SimH for all platforms
cd 02.3_simh/4.x+realcons/src
echo ; echo "*** SimH 4.x for x86"
MAKE_TARGET_ARCH=X86 make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** SimH 4.x for x64"
MAKE_TARGET_ARCH=X64 make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** SimH 4.x for BeagleBoneWhite"
MAKE_TARGET_ARCH=BBW make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** SimH 4.x for BeagleBoneBlack"
MAKE_TARGET_ARCH=BBB make $MAKEOPTIONS $MAKETARGETS
echo ; echo "*** SimH 4.x for RaspberryPi"
MAKE_TARGET_ARCH=RPI make $MAKEOPTIONS $MAKETARGETS
)

echo
echo "All OK!"
