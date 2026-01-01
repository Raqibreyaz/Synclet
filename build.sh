#!/usr/bin/bash

# update the system
sudo apt update
sudo apt upgrade -y

# install necessary build tools
sudo apt install -y git zip unzip build-essential pkg-config

# download vcpkg for c/c++ package management
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh

# add the vcpkg to path variable for system wide access
echo 'export PATH="$HOME/vcpkg:$PATH"' >> ~/.bashrc
source ~/.bashrc
