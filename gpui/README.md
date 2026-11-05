# Fourpole with a GPUI front end (experimental)

This branch (`gpui`) draws and plays Fourpole in a [GPUI](https://gpui.rs) window. `main` keeps the JUCE app as it was.

How it's put together:
- **The sound is unchanged.** The C++ engine is reached through a C interface, `bridge/FourpoleBridge.h`.
- **The panel's logic is unchanged too.** The JUCE panel still runs, off-screen, as the controller: patches, undo, MIDI learn, saving, the Setup menu and the dialogs all behave as in the app.
- **The photoreal parts come from the same renderer.** Knobs (at 120 angles), rockers, lamps, wheels, the keys pressed and unpressed, and the panel are rendered once by the C++ renderer and cached.
- **GPUI does the rest.** It lays the panel out at any window size, composites the parts on the GPU, and draws the name plate and the patch bar. It handles the mouse (drag or scroll knobs and wheels, click rockers, play and glide across the keys) and the computer keyboard (the same layout as the app).

## Building

You need Rust (rustup), CMake, Ninja and the Xcode Command Line Tools. Full Xcode isn't needed: GPUI's shaders compile at runtime.

```sh
scripts/bootstrap.sh            # JUCE into external/JUCE
cd gpui
cargo run --release
```

`build.rs` builds the C++ side (`fourpole_bridge`, a static library with JUCE) with CMake and links it.

## Self-test

Every knob, selector and rocker is worked through the window's own mouse handlers, then a key is clicked and checked for sound:

```sh
FOURPOLE_PATCH_DIR=/tmp/patches FOURPOLE_GPUI_SELFTEST=/tmp/report.txt cargo run --release
```

It changes the panel through the real controls, so it saves the panel's settings as the app does.
