mkdir bin
g++ src/main.cpp --debug -Wall -Wextra -IDataStructures -I/. -lpthread -lm -DNO_BIND -o bin/sleep_server
