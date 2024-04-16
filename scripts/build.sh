cd ../

cd vendor/glew-cmake/

./cmake-testbuild.sh

cd ../../

cd build/

cmake -S ../ -DCMAKE_BUILD_TYPE=Debug
cmake --build .


