# xcb-multi-api-template
A fusion of the OpenGL and Vulkan templates. This is meant to give an example of how two APIs can be switched between inside an application. 

This uses Vulkan by default.

I'm not advanced enough to use both at the same time, so if you were looking into that I'm sorry.
## Build
 * clang
 * glslc
 * XCB development files
 * Vulkan development files
 * OpenGL development files
 * make
 
Run `make`, then `cd build`, and finally `./xcb-multi`
## Options
#### `--use-opengl`
Load OpenGL first.

#### `--use-vulkan`
Load Vulkan first.

#### `--force-opengl`
Load OpenGL and error if it fails.

#### `--force-vulkan`
Load Vulkan and error if it fails.

#### `--vulkan-max-frames-in-flight n`
Replace `n` with any number. This defines how many frames will be rendered at once before waiting for a frame to be presented.

This only affects Vulkan.
