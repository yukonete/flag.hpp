# flag.hpp

C++ header only library for parsing command-line flags

Inspired by Go's [flag package](https://pkg.go.dev/flag)

## Quick Start
See [example.cpp](example.cpp)

## Flag Syntax
Syntax is the same as the syntax of Go's [flag package](https://pkg.go.dev/flag), except that `-flag value` syntax for booleans is allowed

Additionally, there is flag ignore syntax: `-/flag`. This syntax was taken from [flag.h](https://github.com/tsoding/flag.h)

Integer and floating point numbers are parsed using [std::from_chars](https://en.cppreference.com/cpp/utility/from_chars), so supported patterns for them are the same as for that function
