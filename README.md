# Dev-Portal
Everything you need to get started on developing effects for Stratus® and Tone Shop™.


## Required Hardware

- [Stratus®](https://chaosaudio.com/products/stratus) multi-effects pedal


## Resources

[FAUST](https://faust.grame.fr/) - A useful DSP programming language, which we officially recommend for any effects development, and which has integrated support for the Stratus® pedal!

[Online FAUST IDE](https://faustide.grame.fr/) - A web-based IDE for developing and testing Faust-based algorithms. Great for testing effects prior to actually loading them on Stratus®. AND it has intergated support for the Stratus® pedal!

[JUCE](https://juce.com/) - JUCE is a widely used framework for audio application and plug-in development. It is an open source C++ codebase that can be used to create standalone software on Stratus®, Windows, macOS, Linux, iOS, and Android, as well as VST, VST3, AU, AUv3, AAX and LV2 plug-ins.

[Our own FAUST tutorial](https://github.com/chaosaudio/Dev-Portal/wiki/Faust-and-the-Stratus-%E2%80%90-a-basic-tutorial) explaining how to use Faust and the Faust IDE to develop _and install_ Stratus® effects!

## FAUST vs JUCE (or both)

If you're planning on developing effects for Stratus® and Tone Shop™ using JUCE libraries, please see the `juce_effect` example here:
[JUCE Example](https://github.com/chaosaudio/Dev-Portal/tree/main/examples/juce_effect)

If you're planning on using FAUST to develop your effects, continue on through this README!

## Usage

You must include the provided `dsp.hpp` file in any algorithms you compile. You can find this header file in the `resources` folder.

```javascript
import "dsp.hpp"

class example_effect : public dsp {

}
```

*NOTE: When using the integrated Faust and Faust IDE support for the Stratus, this can be disregarded. The C++ code will be generated for you from Faust and be in the correct format out-of-the-box!*

## Compilation with Docker

* Be sure to update the submodules after cloning this repo:

```bash
git submodule update --init --recursive
```

* Now, to build & test new plugins:

```bash
docker buildx create --name mybuilder
docker buildx use mybuilder
docker run -it --rm --privileged tonistiigi/binfmt --install all # Install all qemu emulators
docker buildx build --platform linux/arm -t chaos-audio-builder --load .
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" chaos-audio-builder # Cross-compilation x86_64 to arm/v7
```

* When build is finished, you can run benchmark util in the same container, just like this:

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" --entrypoint bash chaos-audio-builder
cd bins
./tests/benchmark/benchmark-plugin /workdir/bins/equalizer.so 2 44100
Loading library: /workdir/bins/equalizer.so
Version: 2.0.0
Loading symbol: create
Creating instance
Setting sample rate: 44100
Invoking instanceConstants method
Invoking benchmark method
Starting benchmark
Generating input signal
Computing 2 seconds of data @44100Hz
Processed 2 seconds of signal in 0.038303 seconds
52.214863 x real-time
Deleting instance
```

## Compilation on Stratus

SSH into Stratus, copy your files to Stratus' local filesystem, and build your algorithm with the following command:

```bash
  g++ -fPIC -shared -O3 -g -march=armv7-a -mtune=cortex-a8 
-mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only "EFFECT_NAME".cpp -o "EFFECT_NAME".so
```

These flags will ensure that the most optimized binary is generated.   

You can log into Stratus via your terminal with the following command (Mac and Linux only):
```bash
ssh root@stratus.local
```

Please reach out to us via the [Developer Application](https://chaosaudio.com/pages/developer-portal) on our website, or via email at support@chaosaudio.com, to obtain the root user password.

*NOTE: When using the integrated Faust and Faust IDE support for the Stratus, this can be disregarded. The binary file will be built and installed into the proper directory when you execute the appropriate Faust tooling! However, you will still need to reach out to us to obtain the root user password.*

## Testing

To test your effect on the pedal, you'll need the "Beta" version of the Chaos Audio mobile app. You can access the Beta version of the app by joining the Chaos Audio [Beta Program](https://chaosaudio.com/pages/beta-program).
 
In the Beta version of the app, under the "Development" category, you'll find an effect titled "9 KNOB". This effect includes 9 different parameters for testing your own algorithm, all of which have a range of 0 to 10 and a step size of 0.1. You must ensure that your algorithm parameters are mapped to these ranges in order to use the tester effect.

If you are NOT using the integrated Faust features, you must follow these steps:

* Rename your compiled binary to:
  ```bash
  55631e3a-94f7-42f8-8204-f5c6c11c4a21.so
  ```

* Place your binary inside the directory:
  ```bash
  /opt/update/sftp/firmware/effects
  ```

This will allow you to use the "9 KNOB" tester effect in the app to test your algorithm. 
*NOTE: Do not click "install" in the app or it will overwrite your own algorithm with an unrelated one!*

* To see a log of what's happening on the pedal, run the following command on Stratus:
```bash
journalctl -f -u bela_startup.service
```

Again, the integrated support for the Stratus found in Faust and the Faust IDE will perform these steps for you.

You can _also_ test your effect _away_ from the pedal:

* Use Faust to build your effect on your *local* computer (i.e. NOT for the pedal)
* Use the [Chaos Stratus Python wrapper](https://pypi.org/project/chaos-stratus/) to allow you to load the effect into a Python script, interact with the effect's knobs and switches, apply the effect algorithm to raw digital signal data, and listen to the results.

## Public Release to Tone Shop™

The following items are necessary for every effect to be listed on Tone Shop™:

- **Effect name**
    
    This is the name of your effect.
    
- **Effect price**
    
    This is the list price for your effect. If you're interested in charging for your effect, please let us know by emailing support@chaosaudio.com.
    
- **Effect subtitle**
    
    This is a 2-3 word description for your effect.

- **Effect description**
    
    This is the full description for your effect. It should include information on usage, information on any knobs/parameters, and a link to any demos available.
    
- **Effect author**
    
    This is your name or company name.
        
- **Effect version number**
    
    This is the most recent version number for your effect. This MUST always be higher than any previous versions. It should be in the format "0.0.0".
    
- **Effect file**
    
    This is the final, compiled binary for your algorithm. Will have file extension ".so".
    
- **Pedal mockup**
    
    This is an image of your effect fully-built. (i.e. roughly the same size as your "pedal background image", but including all knobs, switches, LEDs, labels, audio jacks, etc in their proper positions)
    
- **Pedal preview card**
    
    This is the square image that will show up in Tone Shop™ as users are browsing through effects. We recommend following the same format as other effects currently in Tone Shop.
    
- **Pedal background image**
    
    We recommend an aspect ratio of 230 x 423 (can be higher resolution than this). Feel free to experiment with other aspect ratios. This background artwork should only include your logo, artwork, and pedal name. It should not include knob labels, audio jacks, knobs, switches, LEDs, etc.
    
- **Knob base image** (for each knob)
    
    This is the image associated with your knob that SHOULD NOT rotate when a user adjusts the knob. (i.e. a shadow or light gradient)
    
- **Knob image** (for each knob)
    
    This is the image associated with your knob that DOES rotate when a user adjusts the knob. (i.e. a tick mark or outer texture)
    
- **Knob label image** (for each knob)
    
    This is the image associated with your knob label (name) that is positioned near the knob it is describing. (i.e. "Drive")
    
- **Stomp switch pressed image**
    
    This is the image associated with your effect bypass switch that displays any time a user touches (presses) the bypass switch. (i.e. it should look like the switch is being pressed)
    
- **Stomp switch released image**
    
    This is the image associated with your effect bypass switch that displays any time a user releases (is not touching) the bypass switch. (i.e. it should look like the switch is not being pressed)
    
- **LED ON image**
    
    This is the image that indicates to the user that the effect is ENABLED. (i.e. an LED that is illuminated)
    
- **LED OFF image**
    
    This is the image that indicates to the user that the effect is BYPASSED. (i.e. an LED that is off)
    
- **Audio jack** (for each jack, usually two)
    
    This is the image that is associated with an audio jack that you would like displayed somewhere on the outside border of your pedal background image.

## Support

For support, email support@chaosaudio.com.

