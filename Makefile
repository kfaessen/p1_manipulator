# Define the compiler
CXX = g++

# Define the flags to pass to the compiler
CXXFLAGS = -std=c++11 -Wall -O2

# Include the Boost library path
BOOST_LIBS = -lboost_system

# Define the output executable name
TARGET = serial_reader_writer

# Define the source files
SRCS = main.cpp

# Define the object files
OBJS = $(SRCS:.cpp=.o)

# Define the default target
all: $(TARGET)

# Rule to build the target executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(BOOST_LIBS)

# Rule to build the object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to clean the build files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets are not actual files
.PHONY: all clean
