# If RACK_DIR is not defined, default to our local SDK location
RACK_DIR ?= dep/Rack-SDK

# Compiler flags
FLAGS += -Isrc -DCOMPUTERCARD_NOIMPL -DVCV_PORT=1

# Plugin source files
SOURCES += src/plugin.cpp
SOURCES += src/WorkshopComputer.cpp

# Distributable assets
DISTRIBUTABLES += res
DISTRIBUTABLES += plugin.json

# Include arch.mk explicitly to get target OS/CPU variables before Makefile.cards
include $(RACK_DIR)/arch.mk

# Include card sources and include paths
include Makefile.cards

# Include the VCV Rack plugin Makefile helper
include $(RACK_DIR)/plugin.mk

# Override standard to C++17 to support modern card features (std::clamp, generic lambdas)
# -Wno-deprecated-declarations: suppress sprintf deprecation warnings in third-party Lua sources
# -Wno-narrowing / -Wno-c++11-narrowing: suppress narrowing warnings in ported card code
CXXFLAGS += -std=c++17 -Wno-narrowing -Wno-deprecated-declarations
ifdef ARCH_WIN
    # Enable ANSI stdio in MinGW for %zu and other C99 format specifiers
    CXXFLAGS += -D__USE_MINGW_ANSI_STDIO=1
    # -Wno-c++11-narrowing is Clang-only; MinGW/GCC uses -Wno-narrowing (already added)
else
    # Clang-only flag for narrowing (no-op on GCC, used on macOS/Clang)
    CXXFLAGS += -Wno-c++11-narrowing
endif

# Link ws2_32 on Windows for the web server
ifdef ARCH_WIN
    LDFLAGS += -lws2_32
endif

cards: $(CARD_LIBS)

$(TARGET): | $(CARD_LIBS)

# Custom clean target for card libraries
clean: clean-cards

clean-cards:
	rm -rf res/cards

# Direct install to the extracted plugin directory (for development - takes effect without VCV restart extraction)
INSTALLED_PLUGIN_DIR = $(HOME)/Library/Application Support/Rack2/plugins-mac-arm64/MTMWorkshopComputer
install-dev: all
	mkdir -p "$(INSTALLED_PLUGIN_DIR)"
	cp plugin.dylib "$(INSTALLED_PLUGIN_DIR)/"
	cp plugin.json "$(INSTALLED_PLUGIN_DIR)/"
	rm -rf "$(INSTALLED_PLUGIN_DIR)/res"
	cp -r res "$(INSTALLED_PLUGIN_DIR)/"
	@echo "Installed directly to $(INSTALLED_PLUGIN_DIR)"

test_behavioral:
	python3 -u tools/test_card_behavioral.py


