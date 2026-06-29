CXXFLAGS=-std=c++20 -Wall -Wextra -pedantic -g -Wconversion -Wsign-conversion

.PHONY: all
all:
	$(CXX) $(CXXFLAGS) -o example example.cpp
