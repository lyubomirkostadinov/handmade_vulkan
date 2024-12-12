ASSEMBLY := engine
BUILD_DIR := bin
GAME_DIR := game

COMPILER_FLAGS := -v -std=c++11
INCLUDE_FLAGS := -Iengine -I$(VULKAN_SDK)/include
LINKER_FLAGS := -L$(VULKAN_SDK)/lib -lvulkan -framework Cocoa -framework QuartzCore -framework Foundation -framework Metal -rpath @loader_path/

SRC_FILES := $(ASSEMBLY)/main.cpp $(ASSEMBLY)/platform_macos.mm
GAME_SRC_FILES := $(GAME_DIR)/game.cpp
GAME_LIB := $(BUILD_DIR)/libgame.dylib

.PHONY: all clean build

# Add a default target
all: build

# Ensure we build the app and the game library
build: $(GAME_LIB) $(BUILD_DIR)/app

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile the game.cpp into a shared library named libgame.dylib
$(GAME_LIB): $(GAME_SRC_FILES) | $(BUILD_DIR)
	clang++ -g $(COMPILER_FLAGS) -shared -fPIC $(GAME_SRC_FILES) -o $(GAME_LIB) -install_name @rpath/libgame.dylib

# Compile the main application, linking with the libgame.dylib library
$(BUILD_DIR)/app: $(SRC_FILES) $(GAME_LIB) | $(BUILD_DIR)
	clang++ -g $(COMPILER_FLAGS) $(SRC_FILES) -o $(BUILD_DIR)/app $(INCLUDE_FLAGS) $(LINKER_FLAGS) -L$(BUILD_DIR) -lgame

# Clean the build directory
clean:
	rm -rf $(BUILD_DIR)
