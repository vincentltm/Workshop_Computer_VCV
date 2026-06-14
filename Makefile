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
CXXFLAGS += -std=c++17 -Wno-narrowing -Wno-c++11-narrowing

# Link ws2_32 on Windows for the web server
ifeq ($(ARCH), win)
    LDFLAGS += -lws2_32
endif

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
