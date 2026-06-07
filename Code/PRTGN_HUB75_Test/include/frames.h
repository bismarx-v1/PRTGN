#ifndef FRAMES_H
#define FRAMES_H
#include <cstdint>

#ifndef PROGMEM
#define PROGMEM
#endif

// Frame 0: Eyes Open (4096 elements for 128x32)
inline const uint16_t frame_open[4096] PROGMEM = {
    // ... hex data for open eyes ...
};


// Frame 1: Eyes Closed (4096 elements for 128x32)
inline const uint16_t frame_closed[4096] PROGMEM = {
    // ... hex data for closed eyes ...
};



#endif // FRAMES_H