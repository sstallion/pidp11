#1/bin/sh
#
#
# install script for PiDP-11
# v0.20180511
#
PATH=/usr/sbin:/usr/bin:/sbin:/bin


apt-get update
#Install SDL2, optionally used for PDP-11 graphics terminal emulation
apt-get install libsdl2-dev
#Install pcap, optionally used when PDP-11 networking is enabled
apt-get install libpcap-dev
#Install readline, used for command-line editing in simh
apt-get install libreadline-dev
# Install screen
apt-get install screen
# Run xhost + at GUI start to allow access for vt11. Proof entire setup needs redoing.
echo xhost + >> /etc/xdg/lxsession/LXDE-pi/autostart


# Set up pidp11 init script, provided pidp11 is installed in /opt/pidp11
if [ ! -x /opt/pidp11/etc/rc.pidp11 ]; then
	echo pidp11 not found in /opt/pidp11. Abort.
	exit 1
else
	ln -s /opt/pidp11/etc/rc.pidp11 /etc/init.d/pidp11
	update-rc.d pidp11 defaults
fi


# setup 'pdp.sh' (script to return to screen with pidp11) in home directory if it is not there yet
test ! -L /home/pi/pdp.sh && ln -s /opt/pidp11/etc/pdp.sh /home/pi/pdp.sh


# add pdp.sh to the end of pi's .profile
#   first, make backup .foo copy...
test ! -f /home/pi/profile.foo && cp -p /home/pi/.profile /home/pi/profile.foo
#   add the line to .profile if not there yet
if grep -xq "/home/pi/pdp.sh" /home/pi/.profile
then
	echo .profile already done, OK.
else
	sed -e "\$a/home/pi/pdp.sh" -i /home/pi/.profile
fi
