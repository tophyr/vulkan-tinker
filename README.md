# Vulkan Tinker Project

This is a simple project I'm using to teach myself Vulkan. It is not intended to be useful in any way, and may very possibly make you dumber for using it.

## Building

This project uses [CMake](https://cmake.org/download/) for builds and [vcpkg](https://github.com/microsoft/vcpkg) for dependency management. It should support Windows/Mac/Linux, and in theory be trivially source-portable to Android.

1. Get [CMake](https://cmake.org/download/) and [vcpkg](https://github.com/microsoft/vcpkg) onto your system somehow.
   > **Note:** If you are developing on Windows, Visual Studio already includes both CMake and vcpkg - use the Developer Tools shells and it will "just work".
2. In order for CMake to pick up the vcpkg configuration, you must set a `VCPKG_ROOT` environment variable to the location of the `vcpkg` installation - it will have a file called `.vcpkg-root`.
   > **Note:** Again, Visual Studio Developer Tools users need not worry about this; it's already set up for you.
3. Run `vcpkg install` to download and build the project dependencies.
4. Run `cmake --preset <YOUR_SELECTED_PRESET>`, selecting the preset you'd like from `CMakePresets.json`.
5. Run `cmake --build out/build/<YOUR_SELECTED_PRESET>`
6. Change directory to `out/build/<YOUR_SELECTED_PRESET>/`. The program expects to find DLLs and shader files in its CWD.
7. Run `vulkan-tinker`