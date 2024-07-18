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

dockerbuild:
	docker build -t sleep_server -f server.dockerfile .
	docker build -t sleep_client -f client.dockerfile .

dockerrun: 
	docker run -d --name server --network bridge -t sleep_server 
	docker run -d --name client0 --network bridge -t sleep_client
	docker run -d --name client1 --network bridge -t sleep_client
	docker run -d --name client2 --network bridge -t sleep_client
	docker run -d --name client3 --network bridge -t sleep_client
	docker run -d --name client4 --network bridge -t sleep_client

dockerclean:
	docker kill server 
	docker kill client0
	docker kill client1
	docker kill client2
	docker kill client3
	docker kill client4
	docker rm server
	docker rm client0
	docker rm client1
	docker rm client2
	docker rm client3
	docker rm client4
	docker image rm sleep_server
	docker image rm sleep_client

dockerrestart:
	docker restart server
	docker restart client0
	docker restart client1
	docker restart client2
	docker restart client3
	docker restart client4

docker: clean all dockerclean dockerbuild dockerrun

	
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