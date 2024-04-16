# BlazeEngine

Engine based on 2d games but i have a plans to do for 3d also.<br>It depend on time and wish.

# Plan

* In plans to create basic Rendering(rendering, texture mapping, animations, shadowing, and lighting)<br>
* Second physics engine(collision detection, rigid and soft body dynamics)<br>
* Third audio and ui in the game <br>
* Then basic scripting and input handler with  event system<br>
* And end of the all this features multiplayer<br>

# Build

First Way:<br>
1) You can go to the script folder then Setup folder and start setup shell script<br>
2) Then find project in buildd folder<br>


Second Way: <br>
Requirements:<br>
* git (new version optional)<br>
* cmake (at-least 3.29.0)<br>

To build you need cmake :

```cmake
git clone --recursive https://github.com/kodzimk/Blaze.git
go to glew-cmake submodule and start cmake-testbuild
then to the main folder and delete all from build folder
then  do this commands:
cd build/
cmake -S ../ -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```
Then go to the build folder and find project thats main project<br>

to build your project without using terminal you can with build shell script(Direction:script/build/)<br>
Good luck!

