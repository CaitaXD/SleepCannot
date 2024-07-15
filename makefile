CXX = g++
CXXFLAGS = --debug -Wall -Wextra -IDataStructures -I/. -lpthread -lm
LDFLAGS =

# Build directory
BUILD_DIR = build

# Source and Object files
SOURCES = $(wildcard src/*.cpp)
OBJECTS = $(SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)

# Target executable name
TARGET = sleep_server

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Non-file targets
.PHONY: all clean