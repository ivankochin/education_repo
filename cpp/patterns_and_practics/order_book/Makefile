current_dir=$(shell pwd)

# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -g -std=c++20

# Source files
SRCS = UnitTests.cpp

# Include folders
INC=-I$(current_dir)/boost_1_85_0

# Output executable name
TARGET = unit_tests

# Build the executable
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(INC) $(SRCS) -o $(TARGET)

# Clean up build artifacts
clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET)
