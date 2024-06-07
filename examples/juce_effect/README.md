## ❔ What is this?
This is an example project that you can use as a template for your effects that use JUCE. It uses CMake for building and supports cross-compiling on a Linux desktop machine. The effect itself just runs a LPF filter from a ```juce_dsp``` module with some extra JUCE classes on top. The idea is to use JUCE in your Stratus effects. JUCE's Projucer doesn't support Stratus, but JUCE comes with CMake which makes using it with Stratus possible.

## 🍋 Why JUCE?
There are two main reasons why you might want to use JUCE in your Stratus effect:
1. You want to use some of JUCE's effects or classes in your project. Perhaps something from their [DSP module](https://docs.juce.com/master/index.html#tag_dsp).
2. You want to use some third-party library that was meant to be used in a JUCE project, but you want it in your Stratus effect.

## 😮 Will it just run on any Stratus?

Unfortunately, making it work on just stock units with old Debian Stretch was trickier than expected. Hence, the units' OS require forced libstdc++ update in order to run effects that were built with JUCE. The problem is that the oldest version of JUCE that came with CMake support already won't work with versions of CMake, GCC and libstdc++ that Debian Stretch has. So the current solution is to install ```libstdc++``` from Debian Bullseye on Stratus with all its dependencies, making the OS halfway updated to Bullseye.

In theory, it is maybe possible to build the effect on a machine with Debian Stretch with carefully installed CMake 3.15 and some old version JUCE that doesn't require C++17 support. It you manage to make it work, let us know!

## 🎚️ Which JUCE modules does it support?
This project includes following modules:

- juce_core
- juce_audio_basics
- juce_audio_formats
- juce_dsp

This seems to be the minimal configuration to use ```juce::dsp``` classes. And those modules should cover most of what you'll want to do on Stratus. If your project requires only basic classes like ```juce::AudioBuffer```, it's possible that you'll only need ```juce_core``` module. You can try to include other modules as well, but keep in mind there's a lot of odd cross-dependencies between different JUCE modules, so you'll have to make sure you include all those extra modules as well. Also, they will likely require third-party libraries installed.

On how to add or remove JUCE modules in your project's config, see "Configuring your CMake" section.

## ❔ Which version of JUCE should I use?
This project can run on JUCE 7.0.6, mostly because it's the latest version that supports CMake 3.18 that Debian Bullseye has. You can choose a different version of JUCE, of course, as long as GCC, CMake and some other dependencies are not broken when you build it and move it to Stratus. Just clone it into the project directory, checkout to the desired version, edit the JUCE/modules/CMakeLists.txt file (more on that in the "Configuring your CMake" section) and you should be good to go.

## 🐋 How to build it with Docker

It's probably the easiest way to do it. There's a Docker file in this example's directory that creates a container with an arm/v7 Debian Bullseye on it and builds the project.

1. Go to your juce_effect directory

``` bash
cd path/to/your/juce_effect
```

2. Build the Docker. You'll only have to do it once.

``` bash
docker buildx build --platform linux/arm/v7 --tag stratus-builder --load .
```

3. (Optional) Configure CMake file for your project. Detailed instructions will be in one of the chapters below.

4. Run the Docker container. It already contains the scripts to build your project.

On Windows:

``` bash
docker run --rm -it --platform linux/arm/v7 --volume "%cd%:/workdir" stratus-builder
```

On Mac/Linux:

``` bash
docker run --rm -it --user $UID --platform linux/arm/v7 --volume "$(pwd):/workdir" stratus-builder
```

Your effect will be in a "juce_effect/build_stratus/effect/" folder. You can copy it to Stratus now.

## 🖥️ How to cross-compile it on a PC

The idea is to use a beefy x86-64 machine to produce an .so file that you can just copy to Stratus later.

1. Get a Linux distro running or your PC. You can just make a virtual machine if you're not on Linux. Using Debian is recommended, but other distros can work as well. As long as those Linux libraries that your builds requires exist on Stratus. This example was tested on Debian Bullseye on a Ryzen7 PC in a VirtualBox VM.

2. Install Cmake. Also, install gcc that builds for Stratus' CPU, which is armhf.
``` bash
sudo apt-get install cmake
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

3. (Optional) Install [JUCE dependencies](https://github.com/juce-framework/JUCE/blob/develop/docs/Linux%20Dependencies.md). This project doesn't require them, but you might need install some of those if you're going to use other JUCE modules. You might also need versions of them for armhf architecture, since this is your target platform.

4. (Optional) Configure CMake file for your project. Detailed instructions will be in one of the chapters below.

6. Build it!

``` bash
cd path/to/your/juce_effect
mkdir build_stratus && cd build_stratus
cmake .. -DCMAKE_TOOLCHAIN_FILE=StratusLinuxPcTooclhain.cmake
cmake --build .
```

Your effect will be in a "juce_effect/build_stratus/effect/" folder. You can copy it to Stratus now.

## 🎛️ How to build it on Stratus

Stratus is good at lots of things, but compiling the code is perhaps not one of them. JUCE is a pretty large library, so building it on Stratus will take a long time. But if you still want to do it, here are the steps.

1. If your Stratus is running Debian Stretch, it's recommended to update your Stratus to Debian Bullseye. [How to check Debian version?](https://www.ionos.com/digitalguide/server/know-how/how-to-check-debian-version/#:~:text=lsb_release%20command,-%E2%80%9Clsb_release%E2%80%9D%20is%20another&text=By%20typing%20%E2%80%9Clsb_release%20%2Da%E2%80%9D%2C%20you%20can%20get%20information,information%2C%20including%20your%20Debian%20version.) You can follow [those steps](https://www.cyberciti.biz/faq/update-upgrade-debian-9-to-debian-10-buster/) to update to Debian Buster, then repeat same steps to update to Bullseye. Or you can keep using Stretch, but you'll have to find a version of JUCE that supports its versions of GCC and CMake.
2. Install GCC and CMake
``` bash
apt-get install gcc g++ cmake
```
3. (Optional) Install [JUCE dependencies](https://github.com/juce-framework/JUCE/blob/develop/docs/Linux%20Dependencies.md). This project doesn't require them, but you might need install some of those if you're going to use other JUCE modules.

4. (Optional) Configure CMake file for your project. Detailed instructions will be in one of the chapters below.

5. Add some swap space on your Stratus. It only has 512 Mb of RAM, so it can run out of RAM while building and fail to build. 512Mb to 1Gb should do the trick. [Here's a random article on how to do it.](https://www.virtono.com/community/tutorial-how-to/how-to-add-swap-space-on-debian-11/)

6. Build it! It might take a while though.
``` bash
cd path/to/your/juce_effect
mkdir build_stratus && cd build_stratus
cmake ..
cmake --build .
```

Your effect will be in a "juce_effect/build_stratus/effect/" folder.

## ⚙️ Configuring your CMake

For your own projects, you might want to edit the CMakeLists.txt file.

Things you might want to change:

* Your effect version, see ```EFFECT_VERSION``` macro.

* Your effect GUID, see ```EFFECT_GUID``` macro. You can use GUID of one of the pedal templates while you're starting out, than create your own entry in Chaos Audio's online tool and use the GUID that was assigned for your effect.

* Included JUCE modules. Find a piece that looks like this:

``` cmake
target_link_libraries(${EFFECT_NAME}
    PRIVATE
        juce::juce_audio_basics
        # juce::juce_audio_devices
        ...
```

... and uncomment the ones you need. ❗**Important**❗ You'll need to then edit JUCE/modules/CMakeLists.txt and JuceHeader.h to match those changes.

* Your cpp files list. Find a piece that looks like this:

``` cmake
target_sources(${EFFECT_NAME}
    PRIVATE
        juce_effect.cpp
    )
```

...and edit it. JUCE's sources are included automatically, you don't need to add them here.

* GCC build flags. Find a line that looks like this in CMakeLists or ...Toolchain.cmake, depending on how you build it:

``` cmake
add_compile_options(-fPIC -shared ...)
```

There are a few optional GCC flags you can add or drop, also you can change optimization levels (-O...) to optimize for performance or file size.

## 🎸 Trying out your effect

1. Get Templates -> 1KNOB effect in dev app's Tone Shop

2. Connect to Stratus via SFTP

3. Copy your .so file to /opt/update/sftp/firmware/effects directory. Doing so can crash its firmware, so if the LEDs start blinking, you can manually re-launch it:

``` bash
cd /opt/update/sftp/firmware/ && ./fw
```

4. Add the 1KNOB effect to your pedalboard. Enjoy!
