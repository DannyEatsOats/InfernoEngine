rm -rf build-release/*
cmake -DCMAKE_BUILD_TYPE=Release -B build-release -S .
./ShaderCompile
make -C build-release -j8
./build-release/bin/Release/Game/Game
