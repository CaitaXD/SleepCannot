CXX = g++
CXXFLAGS = --debug -Wall -Wextra -lpthread -lm
LDFLAGS =

# Build directory
BUILD_DIR = build
BIN_DIR = bin

# Source and Object files
SOURCES = $(wildcard src/*.cpp)
OBJECTS = $(SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)

# Target executable name
TARGET = $(BIN_DIR)/sleep_server

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Non-file targets
.PHONY: all clean