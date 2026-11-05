/* Fourpole for the GPUI front end: a C interface to the instrument (the C++ engine and its panel, kept
   off-screen as the controller and as the renderer of the panel's parts). Everything here is called on the
   main thread, which is also JUCE's message thread. Images are premultiplied BGRA, 4 bytes a pixel. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FpPanel FpPanel;

/* Starts JUCE, opens the audio device and MIDI, and builds the panel at this size (logical pixels). */
FpPanel* fp_create (int width, int height);
void fp_destroy (FpPanel*);

/* The panel's parts as JSON: [{"index","kind","id","x","y","w","h","min","max","steps"}...] plus the name plate
   and the patch bar. Kinds: knob, selector, rocker, lamp, wheel, keyboard. Valid until the next call. */
const char* fp_parts_json (FpPanel*);
/* The panel with no parts on it (w x h pixels = the panel at `scale`). Returns 0 on success. */
int fp_render_background (FpPanel*, float scale, uint8_t* bgra, int w, int h);
/* Part `index` drawn at `value` (knob value, selector position, rocker 0/1, lamp 0/1, wheel value). For the
   keyboard, value is a note: that key drawn pressed, cropped to the key (-1: all keys up). w x h must be the
   part's (or the key's) size at `scale`. */
int fp_render_part (FpPanel*, int index, float value, float scale, uint8_t* bgra, int w, int h);
/* A key's rectangle within the keyboard part (logical px). */
void fp_key_rect (FpPanel*, int note, float* x, float* y, float* w, float* h);
int fp_key_is_black (int note);

/* Controls, by panel ID. */
float fp_get (FpPanel*, const char* id);
void fp_set (FpPanel*, const char* id, float value);

/* Playing. */
void fp_note_on (FpPanel*, int note, int velocity);
void fp_note_off (FpPanel*, int note);
void fp_set_wheel (FpPanel*, int which, float value); /* 0 pitch (-1..1), 1 mod (0..1) */
/* The computer keyboard: a character pressed or let go (the original panel's piano layout, Z/X octave,
   arrows for patches, Escape for all notes off). Returns 1 if it was used. */
int fp_key_event (FpPanel*, const char* chars, int down, int command, int shift);

/* What's live: patch name, message, lamps, wheels, notes held, recording (JSON). */
const char* fp_state_json (FpPanel*);

/* The patch bar. */
void fp_command (FpPanel*, const char* name); /* prev, next, save, saveAs, load, setup, audio, help, rec */

/* Where the panel is on screen, so its menus and pop-ups open over it: the window's content (its top left in screen
   points; or worked out from a real mouse event at x, y in the window), and the panel in the window (its top left in
   window points, and window points per panel pixel). */
void fp_window_origin (FpPanel*, float x, float y);
void fp_window_origin_from_pointer (FpPanel*, float x, float y);
void fp_place (FpPanel*, float x, float y, float scale);
/* What the part under x, y (panel pixels) is, for a tooltip ("" if nothing says). */
const char* fp_tooltip (FpPanel*, float x, float y);
/* (the self-test) 0: no menu or window open; 1: one is open over the panel; 2: elsewhere. And closing them. */
int fp_menu_check (FpPanel*);
void fp_close_menus (FpPanel*);

#ifdef __cplusplus
}
#endif
