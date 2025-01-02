# Makefile for 2D Racing Game

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LIBS = -lsfml-graphics -lsfml-window -lsfml-system

TARGET = race
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) *.o
