
#!/bin/bash

# this calls a much-simplified version of the impenetrable simh makefile
# however - it's simplified by me... you can still use the regular makefile from simh/BlinkenBone

cd ./02.3_simh/4.x+realcons/src
sudo make -f quickmake
cd ../../..
