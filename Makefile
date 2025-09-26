# RayWhen Raycasting Engine Makefile
# GCC-only build system

# Compiler and flags
CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -luser32 -lgdi32 -lcomdlg32 -lpsapi

# Directories
SRC_DIR = src
DIST_DIR = dist
MAPS_DIR = maps
ASSETS_DIR = assets

# Source files
SOURCES = $(SRC_DIR)/raywin.c \
          $(SRC_DIR)/texture.c \
          $(SRC_DIR)/map.c \
          $(SRC_DIR)/player.c \
          $(SRC_DIR)/enemy.c \
          $(SRC_DIR)/renderer.c

LAUNCHER_SOURCES = $(SRC_DIR)/launcher.c
MAPEDIT_SOURCES = $(SRC_DIR)/mapedit.c

# Object files
OBJECTS = $(SOURCES:.c=.o)
LAUNCHER_OBJECTS = $(LAUNCHER_SOURCES:.c=.o)
MAPEDIT_OBJECTS = $(MAPEDIT_SOURCES:.c=.o)

# Executables
MAIN_EXE = $(DIST_DIR)/raywin.exe
LAUNCHER_EXE = $(DIST_DIR)/launcher.exe
MAPEDIT_EXE = $(DIST_DIR)/mapedit.exe

# Default target
all: directories $(MAIN_EXE) $(LAUNCHER_EXE) $(MAPEDIT_EXE) copy_assets copy_maps

# Create necessary directories
directories:
	@mkdir -p $(DIST_DIR)
	@mkdir -p $(DIST_DIR)/$(MAPS_DIR)

# Main game executable
$(MAIN_EXE): $(OBJECTS)
	@echo "=== Compiling Advanced Raycasting Engine with GCC ==="
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "=== Compilation successful! ==="

# Launcher executable
$(LAUNCHER_EXE): $(LAUNCHER_OBJECTS)
	@echo "=== Compiling Launcher ==="
	$(CC) $(LAUNCHER_OBJECTS) -o $@ $(LDFLAGS)
	@echo "=== Launcher compilation successful! ==="

# Map editor executable
$(MAPEDIT_EXE): $(MAPEDIT_OBJECTS)
	@echo "=== Compiling Map Editor ==="
	$(CC) $(MAPEDIT_OBJECTS) -o $@ $(LDFLAGS)
	@echo "=== Map Editor compilation successful! ==="

# Compile source files to object files
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Copy assets to dist directory
copy_assets:
	@echo "=== Copying Assets to Dist ==="
	@if [ -d "$(ASSETS_DIR)" ]; then \
		cp -r "$(ASSETS_DIR)" "$(DIST_DIR)/"; \
	fi
	@echo "Assets copied!"

# Copy maps to dist directory
copy_maps:
	@echo "=== Copying Maps to Dist ==="
	@if [ -d "$(MAPS_DIR)" ]; then \
		cp "$(MAPS_DIR)"/*.rwm "$(DIST_DIR)/$(MAPS_DIR)/" 2>/dev/null || true; \
		cp "$(MAPS_DIR)"/*.txt "$(DIST_DIR)/$(MAPS_DIR)/" 2>/dev/null || true; \
	fi
	@echo "Maps copied!"

# Clean build artifacts
clean:
	@echo "=== Cleaning Build Artifacts ==="
	@rm -f $(SRC_DIR)/*.o
	@rm -rf $(DIST_DIR)
	@echo "Clean complete!"

# Clean only object files
clean-obj:
	@echo "=== Cleaning Object Files ==="
	@rm -f $(SRC_DIR)/*.o
	@echo "Object files cleaned!"

# Rebuild everything
rebuild: clean all

# Run the launcher
run: all
	@echo "=== Launching launcher.exe ==="
	$(LAUNCHER_EXE)

# Run the main game directly
run-game: all
	@echo "=== Launching raywin.exe ==="
	$(MAIN_EXE)

# Run the map editor
run-editor: all
	@echo "=== Launching mapedit.exe ==="
	$(MAPEDIT_EXE)

# Check if GCC is available
check-gcc:
	@echo "=== Checking GCC Compiler ==="
	@gcc --version > /dev/null 2>&1; \
	if [ $$? -ne 0 ]; then \
		echo "Error: GCC compiler not found! Please install MinGW-w64 or MSYS2."; \
		echo "Download from: https://www.msys2.org/ or https://mingw-w64.org/"; \
		exit 1; \
	fi
	@echo "GCC compiler found!"

# Show build information
info:
	@echo "=== RayWhen Build Information ==="
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Linker Flags: $(LDFLAGS)"
	@echo "Source Directory: $(SRC_DIR)"
	@echo "Distribution Directory: $(DIST_DIR)"
	@echo ""
	@echo "=== Features: ==="
	@echo "- Window resizing support"
	@echo "- DDA raycasting algorithm"
	@echo "- Texture mapping with procedural textures"
	@echo "- Collision detection"
	@echo "- Minimap display"
	@echo "- Smooth movement controls (WASD/Arrows)"
	@echo "- Multiple wall types with different colors"
	@echo "- Distance-based shading"
	@echo "- Performance mode for lower-end hardware"

# Help target
help:
	@echo "=== RayWhen Makefile Help ==="
	@echo "Available targets:"
	@echo "  all          - Build all executables and copy assets (default)"
	@echo "  clean        - Remove all build artifacts and dist directory"
	@echo "  clean-obj    - Remove only object files"
	@echo "  rebuild      - Clean and build everything"
	@echo "  run          - Build and run the launcher"
	@echo "  run-game     - Build and run the main game directly"
	@echo "  run-editor   - Build and run the map editor"
	@echo "  check-gcc    - Verify GCC compiler is available"
	@echo "  info         - Show build configuration information"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make         - Build everything"
	@echo "  make clean   - Clean build artifacts"
	@echo "  make run     - Build and launch the game"

# Phony targets
.PHONY: all directories copy_assets copy_maps clean clean-obj rebuild run run-game run-editor check-gcc info help

# Default target
.DEFAULT_GOAL := all
