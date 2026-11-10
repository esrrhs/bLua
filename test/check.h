#pragma once
#include <stdexcept>
#include <string>
#define CHECK(expr) do { if (!(expr)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + #expr); } while (false)
