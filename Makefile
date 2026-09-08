.PHONY: all build test run prove prove-esp32 benchmark clean

all: build

build:
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build build

test: build
	ctest --test-dir build --output-on-failure

run: build
	./build/examples/linux/mcp_server/mcp-server --transport all

prove:
	./scripts/prove.sh

prove-esp32:
	./scripts/prove-esp32.sh "$${MCP_DEVICE_IP:?set MCP_DEVICE_IP to the board address}"

benchmark:
	./scripts/benchmark.sh

clean:
	cmake -E remove_directory build
