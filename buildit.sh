rm -rf build
cmake -S ./src -B build -DCMAKE_BUILD_TYPE=Release
cd build
make