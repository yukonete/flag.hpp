CXXFLAGS=-std=c++23 -Wall -Wextra -pedantic -g -Wconversion -Wsign-conversion

.PHONY: all
all:
	$(CXX) $(CXXFLAGS) -o example example.cpp
