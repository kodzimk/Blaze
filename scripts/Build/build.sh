cd ../../

cd build/

rm -r *

cmake -S ../ -DCMAKE_BUILD_TYPE=Debug
cmake --build .