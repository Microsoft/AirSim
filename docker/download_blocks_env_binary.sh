#!/bin/bash

if ! which unzip; then
    sudo apt-get install unzip
fi

wget -c https://github.com/microsoft/AirSim/releases/download/v1.4.0-linux/Blocks.zip
unzip Blocks.zip
rm Blocks.zip
