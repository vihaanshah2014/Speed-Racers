# Makefile

# Compiler
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# SFML Libraries
SFML_LIBS = -lsfml-graphics -lsfml-window -lsfml-system

# Target
TARGET = race

# Source Files
SRCS = main.cpp

# Object Files
OBJS = $(SRCS:.cpp=.o)

# Build Target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(SFML_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean Up
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
