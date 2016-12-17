#define CATCH_CONFIG_MAIN

#include "copy_on_write.h"
#include <catch.hpp>


TEST_CASE("Default constructed copy_on_write is empty", "[copy_on_write]")
{
  copy_on_write<int> c;
  REQUIRE_FALSE(bool(c));
}

