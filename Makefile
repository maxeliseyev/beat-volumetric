CONFIG ?= release

.PHONY: all debug configure build test bench

all: test

debug:
	$(MAKE) CONFIG=debug all

configure:
	cmake --preset $(CONFIG)

build: configure
	cmake --build --preset $(CONFIG)

test: build
	ctest --preset $(CONFIG)

bench: build
	cmake "-DRUNNER=$(CURDIR)/build/$(CONFIG)/tools/beat_leveler_runner" "-DOUTPUT_DIR=$(CURDIR)/build/$(CONFIG)/bench" -P scripts/bench.cmake
