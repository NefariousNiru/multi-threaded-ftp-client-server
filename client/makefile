# Compiler
CXX = g++

# Compiler Flags
CXXFLAGS = -Wall -Wextra -g -pthread -std=c++11 -Iheaders

# Target Executable
TARGET = myftp

# Source Files
SRCS = myftp.cpp

# Object Files
OBJS = $(SRCS:.cpp=.o)

# Default Rule: Build the executable
all: $(TARGET) clean_objects

# Link object files to create the final executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Rule to compile .cpp files to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Remove only the object files
clean_objects:
	rm -f $(OBJS)

# Clean up build artifacts, including the executable
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets (not actual files)
.PHONY: all clean clean_objects
