# Dev-Portal
Everything you need to get started on developing effects for Stratus® and Tone Shop™.


## Required Hardware

- [Stratus](https://chaosaudio.com/products/stratus) multi-effects pedal


## Resources

[FAUST](https://faust.grame.fr/) - A useful DSP programming language, which we officially recommend for any effects development.

[faust-stratus](https://github.com/bassmanitram/faust-stratus) - A toolkit that simplifies the use of Faust in designing algorithms to be used for Chaos Stratus pedal effects, created by Martin Bartlett. *HIGHLY recommended!*

[Online FAUST IDE](https://faustide.grame.fr/) - A web-based IDE for developing and testing Faust-based algorithms. Great for testing effects prior to actually loading them on Stratus.

## Usage

You must include the provided `dsp.hpp` file in any algorithms you compile. You can find this header file in the `resources` folder.

```javascript
import "dsp.hpp"

class example_effect : public dsp {

}
```

*NOTE: When using the [faust-stratus](https://github.com/bassmanitram/faust-stratus) toolkit, this can be disregarded. The C++ code will be generated for you from Faust and be in the correct format out-of-the-box!

## Compilation

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

*NOTE: When using the [faust-stratus](https://github.com/bassmanitram/faust-stratus) toolkit in "Embedded" mode, this can be disregarded. The binary file will be built for you and installed into the proper directory! However, you will still need to reach out to us to obtain the root user password.

## Testing

To test your effect, you'll need the "Beta" version of the Chaos Audio mobile app. You can access the Beta version of the app by joining the Chaos Audio [Beta Program](https://chaosaudio.com/pages/beta-program).
 
In the Beta version of the app, under the "Development" category, you'll find an effect titled "9 KNOB". This effect includes 9 different parameters for testing your own algorithm, all of which have a range of 0 to 10 and a step size of 0.1. You must ensure that your algorithm parameters are mapped to these ranges in order to use the tester effect.
 
Rename your compiled binary to:
```bash
55631e3a-94f7-42f8-8204-f5c6c11c4a21.so
```

Place your binary inside the directory:
```bash
/opt/update/sftp/firmware/effects
```

This will allow you to use the "9 KNOB" tester effect in the app to test your algorithm. 
NOTE: Do not click "install" in the app or it will overwrite your own algorithm with an unrelated one!
 
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

