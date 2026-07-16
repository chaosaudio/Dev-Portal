// THIS IS NOT THE PLUGIN HEADER.
//
// The one and only authoritative ABI header is  resources/dsp.hpp  — it is
// kept byte-identical to the firmware's own copy. An outdated v1 header used
// to live here and silently produced ABI-incompatible plugins when included
// by mistake; it now forwards to the real one.
#include "../resources/dsp.hpp"
