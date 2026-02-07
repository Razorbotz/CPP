#!/bin/bash
#Opens Files, fill in with your path and with the files you want to open
if [[ "$1" == "" ]]; then
cd /mnt/c/Users/mahan/Capstone/SoftwareDevelopment/C++/robotcontrollerclient/src || exit 1
code BinaryMessage.cpp
code control.cpp
code Speedometer.cpp
code ConfigDefinitions.cpp
fi

#Opens up the dashboard, also change the path
if [[ "$1" == "--dash" ]]; then
    cd /mnt/c/Users/mahan/Capstone/SoftwareDevelopment/C++/robotcontrollerclient/build || exit 1
    export GDK_BACKEND=x11
    ./control --init --wsl
fi

#If you don't have the make file yet
#also to allow this script to run use
#chmod +x run.sh
if [[ "$1" == "--first" ]]; then
    cd /mnt/c/Users/mahan/Capstone/SoftwareDevelopment/C++/robotcontrollerclient
    mkdir build
    cd build
    cmake ..
    make
fi

if [[ "$1" == "--change" ]]; then
    cd /mnt/c/Users/mahan/Capstone/SoftwareDevelopment/C++/robotcontrollerclient/build
    cmake ..
    make
fi
