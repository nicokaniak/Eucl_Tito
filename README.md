# Installation

## Installation on Ubuntu

This plugin uses Projucer 8.0.12.

1. Check what version comes with your distribution:
~~~
apt-cache policy juce-tools
~~~
2. If it is 8.0.12, you can install it with:
~~~
sudo apt update && sudo apt install juce-tools juce-modules-source
~~~
3If it is not 8.0.12, download that version from [here](https://github.com/juce-framework/JUCE/releases/tag/8.0.12) and unzip it to ~/no_backup/bin/juce-8.0.12

4. Check if Projucer works, go to the directory ~/no_backup/bin/juce-8.0.12 and run `./Projucer`

5. Install build tools and dependencies:
~~~
sudo apt update && sudo apt install build-essential cmake g++ libasound2-dev libfreetype6-dev libfontconfig1-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev
~~~
6. Depending on the features used by the plugin, you may also need to install additional packages. Please refert to this [guide](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md) for a list of all possible packages.

## Compiling the plugin

1. Go into the directory <Projet>/Builds/LinuxMakeFiles/
2. Run the command `cmake ../..`
2. cmake -B . -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
4Compile the plugin:
~~~
cmake --build build --config Release
~~~
4. Run the plugin:
~~~
TODO
~~~