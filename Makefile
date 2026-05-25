CXXFLAGS=-std=c++20 -Wall -Wextra -pedantic -g

.PHONY: all
all:
	$(CXX) $(CXXFLAGS) -o example example.cpp
