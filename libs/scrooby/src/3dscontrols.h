#ifndef SCROOBY_3DS_CONTROLS_H
#define SCROOBY_3DS_CONTROLS_H

#include <stddef.h>
#include <string.h>

// Applied once while loading a text bible, never during the render loop.
namespace Scrooby3DSControls
{
struct Entry { const char* key; const char* text; };
static const Entry entries[] = {
    {"TUTORIAL_000", "TO DESTROY WASP CAMERAS, JUMP WITH A, THEN PRESS Y TO KICK."},
    {"TUTORIAL_002", "MOVE WITH THE CIRCLE PAD.\\PRESS A TO JUMP.\\HOLD B TO RUN.\\HOLD L + CIRCLE PAD TO MOVE THE CAMERA."},
    {"TUTORIAL_002_(PS2)", "MOVE WITH THE CIRCLE PAD.\\PRESS A TO JUMP.\\HOLD B TO RUN."},
    {"TUTORIAL_003", "PRESS A TO ACCELERATE.\\STEER WITH THE CIRCLE PAD.\\PRESS B TO BRAKE OR REVERSE.\\PRESS Y OR R TO HANDBRAKE."},
    {"TUTORIAL_003_(GC)", "A: ACCELERATE. CIRCLE PAD: STEER. B: BRAKE / REVERSE. Y OR R: HANDBRAKE."},
    {"TUTORIAL_003_(PS2)", "A: ACCELERATE. CIRCLE PAD: STEER. B: BRAKE / REVERSE. Y OR R: HANDBRAKE."},
    {"TUTORIAL_004", "PRESS X TO ENTER OR EXIT A CAR."},
    {"TUTORIAL_009", "PRESS X TO USE THE PHONE BOOTH."},
    {"TUTORIAL_010", "PRESS X TO ENTER OR EXIT A CAR."},
    {"TUTORIAL_011", "PRESS Y TO KICK THE BOX."},
    {"TUTORIAL_013", "PRESS X TO GO INSIDE."},
    {"TUTORIAL_018", "PRESS X TO ENTER OR EXIT A CAR."},
    {"PRESS_START", "PRESS START"},
};
static inline unsigned Hash(const char* key, unsigned modulo)
{
    unsigned value = 0;
    if(!modulo) return 0;
    while(*key) value = (64u * value + (unsigned char)*key++) % modulo;
    return value;
}
static inline bool Equals(const unsigned short* text, const char* ascii)
{
    while(*text && *ascii && *text == (unsigned char)*ascii) { ++text; ++ascii; }
    return !*text && !*ascii;
}
static inline const char* Replacement(unsigned hash, unsigned modulo, const unsigned short* original)
{
    for(size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i)
        if(Hash(entries[i].key, modulo) == hash) return entries[i].text;
    // Exact keycap captions only: narrative and mission text are untouched.
    if(Equals(original, "[ESC]")) return "B";
    if(Equals(original, "[ENTER]")) return "A";
    if(Equals(original, "[ESC] Quit to Main Menu")) return "B: Main Menu";
    return NULL;
}
}
#endif
