# Default target
all: build

# Create build directory and run CMake
configure:
	mkdir -p build
	cd build && cmake ..

# Compile the project using all available CPU cores
build: configure
	cd build && make -j$$(nproc)

# The "run" target: builds first, then sets DISPLAY and executes
run: build
	export DISPLAY=:0 && cd build && ./NovaEngine

