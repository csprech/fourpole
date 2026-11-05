// Builds the C++ side (the bridge library: Fourpole's engine and panel, with JUCE) with CMake, and links it.
fn main() {
    let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).parent().unwrap().to_path_buf();
    for dir in ["src", "bridge"] {
        println!("cargo:rerun-if-changed={}", root.join(dir).display());
    }
    println!("cargo:rerun-if-changed={}", root.join("CMakeLists.txt").display());
    let dst = cmake::Config::new(&root)
        .generator("Ninja")
        .define("FOURPOLE_BUILD_BRIDGE", "ON")
        .define("FOURPOLE_BUILD_APP", "OFF")
        .define("CMAKE_BUILD_TYPE", "Release")
        .build_target("fourpole_bridge")
        .build();
    println!("cargo:rustc-link-search=native={}", dst.join("build").display());
    println!("cargo:rustc-link-lib=static=fourpole_bridge");
    for fw in [
        "Accelerate", "AppKit", "AudioToolbox", "Carbon", "Cocoa", "CoreAudio", "CoreAudioKit", "CoreFoundation", "CoreGraphics",
        "CoreImage", "CoreMIDI", "CoreServices", "CoreText", "Foundation", "IOKit", "Metal", "MetalKit", "QuartzCore", "Security",
        "UniformTypeIdentifiers", "AVFoundation", "AVKit",
    ] {
        println!("cargo:rustc-link-lib=framework={fw}");
    }
    println!("cargo:rustc-link-lib=dylib=c++");
}
