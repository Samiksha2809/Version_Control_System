# Makefile for mygit server

# Compiler
CXX = g++

# Compiler flags


# Libraries
LIBS = -lssl -lcrypto -lz

# Source file
SRC = Server.cpp

# Output executable
TARGET = mygit

# Build target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) -o $(TARGET) $(SRC) $(LIBS)

# Clean up
clean:
	rm -f $(TARGET)

.PHONY: all clean
