rm -rf build_test
cmake -S ./test -B build_test -DCMAKE_BUILD_TYPE=Release
cd build_test
make

./risk_tests
