# Dev-Portal
Everything you need to get started on developing effects for Stratus® and Tone Shop™.


## Documentation

[FAUST](https://faust.grame.fr/) - A useful DSP programming language. When using FAUST to generate C++ code, it will already follow the general format necessary for Stratus.


## Usage

You must include the provided `dsp.hpp` file in any algorithms you compile. You can find this header file in the `resources` folder.

```javascript
import "dsp.hpp"

class example_effect : public dsp {

}
```


## Compilation

SSH into Stratus, copy your files to Stratus' local filesystem, and build your algorithm with the following command:

```bash
  g++ -fPIC -shared -O3 -g -march=armv7-a -mtune=cortex-a8 
-mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math "EFFECT_NAME".cpp -o "EFFECT_NAME".so
```

These flags will ensure that the most optimized binary is generated.   

## Support

For support, email support@chaosaudio.com.

