cd "C:\Files\Programming\C++\vulkan-rendering-engine\template"
rmdir /s /q build
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE="C:\Files\Programming\C++\vcpkg-master\scripts\buildsystems\vcpkg.cmake" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
