// DISABLED: Zone I/O tests for modern Zone implementation
// The new Zone class uses direct geoson/geotiv file I/O instead of the old zone_io module

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("Zone I/O placeholder") {
    // Placeholder test to avoid empty test file
    CHECK(true);
}