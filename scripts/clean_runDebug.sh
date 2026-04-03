rm -rf build-debug/*
cmake -DCMAKE_BUILD_TYPE=Debug -B build-debug -S .
./ShaderCompile
make -C build-debug -j8
./build-debug/bin/Debug/Game/Game
