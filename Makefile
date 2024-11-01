ASSEMBLY := engine
BUILD_DIR := bin

COMPILER_FLAGS := -v -std=c++11
INCLUDE_FLAGS := -Iengine -I$(VULKAN_SDK)/include
LINKER_FLAGS := -L$(VULKAN_SDK)/lib -lvulkan -framework Cocoa -framework QuartzCore -framework Foundation -framework Metal

# Explicitly specify just the main.cpp for simplicity if it's the only file.
SRC_FILES := $(ASSEMBLY)/main.cpp $(ASSEMBLY)/platform_macos.mm

.PHONY: build
build: $(BUILD_DIR)
	clang++ $(COMPILER_FLAGS) $(SRC_FILES) -o $(BUILD_DIR)/app $(INCLUDE_FLAGS) $(LINKER_FLAGS)

# Create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
