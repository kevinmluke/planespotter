# Host-side tests and screen previews (macOS / Linux).
#   make            run all tests
#   make previews   render every screen to docs/previews/*.png
#
# Needs: a C++17 compiler, Python 3 with sgp4, skyfield and Pillow,
# and `pio run` once so the libraries exist in .pio/libdeps.

ROOT := ../..
LIBS := $(ROOT)/.pio/libdeps/planespotter
PY ?= $(shell command -v $(HOME)/.platformio/penv/bin/python || command -v python3)

CXX ?= c++
# Some macOS Command Line Tools installs lack libc++ headers in the toolchain dir;
# point at the SDK copy explicitly when that is the case.
SDK := $(shell xcrun --show-sdk-path 2>/dev/null)
ifneq ($(SDK),)
  STDINC := -nostdinc++ -isystem $(SDK)/usr/include/c++/v1
endif
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter $(STDINC)

CORE := $(ROOT)/src/core/geo.cpp $(ROOT)/src/core/sgp4.cpp $(ROOT)/src/core/iss.cpp \
        $(ROOT)/src/core/aircraft.cpp $(ROOT)/src/core/text.cpp $(ROOT)/src/core/i18n.cpp \
        $(ROOT)/src/core/santa.cpp $(ROOT)/src/core/timing.cpp
GEN := $(ROOT)/src/generated/aircraft_db.cpp $(ROOT)/src/generated/silhouettes.cpp $(ROOT)/src/generated/worldmap.cpp
GFX := $(LIBS)/Adafruit\ GFX\ Library/Adafruit_GFX.cpp $(LIBS)/U8g2_for_Adafruit_GFX/src/U8g2_for_Adafruit_GFX.cpp \
       $(LIBS)/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c $(LIBS)/QRCode/src/qrcode.c
GFXINC := -Ishim -I"$(LIBS)/Adafruit GFX Library" -I$(LIBS)/U8g2_for_Adafruit_GFX/src -I$(LIBS)/QRCode/src -DARDUINO=100

.PHONY: all test previews clean
all: test

build:
	mkdir -p build/previews

build/unit_tests: unit_tests.cpp $(CORE) $(GEN) | build
	$(CXX) $(CXXFLAGS) -o $@ unit_tests.cpp $(CORE) $(GEN)

build/astro_dump: astro_dump.cpp $(CORE) | build
	$(CXX) $(CXXFLAGS) -o $@ astro_dump.cpp $(CORE)

build/preview: preview.cpp $(CORE) $(GEN) $(ROOT)/src/ui/screens.cpp $(ROOT)/src/ui/model.h | build
	$(CXX) $(CXXFLAGS) -w $(GFXINC) -x c -std=c99 -c $(LIBS)/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c -o build/u8g2_fonts.o
	$(CXX) $(CXXFLAGS) -w $(GFXINC) -x c -std=c99 -c $(LIBS)/QRCode/src/qrcode.c -o build/qrcode.o
	$(CXX) $(CXXFLAGS) $(GFXINC) -o $@ preview.cpp $(ROOT)/src/ui/screens.cpp $(CORE) $(GEN) \
	  "$(LIBS)/Adafruit GFX Library/Adafruit_GFX.cpp" $(LIBS)/U8g2_for_Adafruit_GFX/src/U8g2_for_Adafruit_GFX.cpp \
	  build/u8g2_fonts.o build/qrcode.o

test: build/unit_tests build/astro_dump
	./build/unit_tests
	$(PY) test_astro.py

previews: build/preview
	mkdir -p build/previews
	rm -f build/previews/*.pbm
	./build/preview
	$(PY) render_previews.py

clean:
	rm -rf build
