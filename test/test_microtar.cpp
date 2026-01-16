#include <doctest/doctest.h>
#include <string>
#include <filesystem>
#include "microtar/microtar.hpp"

TEST_CASE("microtar header-only create and write") {
    const char* test_filename = "/tmp/test_microtar_basic.tar";
    mtar_t tar;
    
    // Test opening for write
    CHECK(mtar_open(&tar, test_filename, "w") == MTAR_ESUCCESS);
    
    // Test writing file header and data
    std::string test_data = "Hello, microtar header-only!";
    CHECK(mtar_write_file_header(&tar, "test.txt", test_data.size()) == MTAR_ESUCCESS);
    CHECK(mtar_write_data(&tar, test_data.c_str(), test_data.size()) == MTAR_ESUCCESS);
    
    // Test finalize and close
    CHECK(mtar_finalize(&tar) == MTAR_ESUCCESS);
    CHECK(mtar_close(&tar) == MTAR_ESUCCESS);
    
    // Verify file was created
    CHECK(std::filesystem::exists(test_filename));
    
    // Cleanup
    std::filesystem::remove(test_filename);
}

TEST_CASE("microtar header-only read from archive") {
    const char* test_filename = "/tmp/test_microtar_read.tar";
    mtar_t tar;
    
    // First create an archive to read from
    CHECK(mtar_open(&tar, test_filename, "w") == MTAR_ESUCCESS);
    std::string test_data = "Hello, microtar header-only!";
    CHECK(mtar_write_file_header(&tar, "test.txt", test_data.size()) == MTAR_ESUCCESS);
    CHECK(mtar_write_data(&tar, test_data.c_str(), test_data.size()) == MTAR_ESUCCESS);
    CHECK(mtar_finalize(&tar) == MTAR_ESUCCESS);
    CHECK(mtar_close(&tar) == MTAR_ESUCCESS);
    
    // Now test reading from the archive
    CHECK(mtar_open(&tar, test_filename, "r") == MTAR_ESUCCESS);
    
    // Test finding file
    mtar_header_t header;
    CHECK(mtar_find(&tar, "test.txt", &header) == MTAR_ESUCCESS);
    CHECK(header.size == 28); // Length of "Hello, microtar header-only!"
    CHECK(std::string(header.name) == "test.txt");
    
    // Test reading data
    std::string read_data(header.size, '\0');
    CHECK(mtar_read_data(&tar, &read_data[0], header.size) == MTAR_ESUCCESS);
    CHECK(read_data == "Hello, microtar header-only!");
    
    CHECK(mtar_close(&tar) == MTAR_ESUCCESS);
    
    // Cleanup
    std::filesystem::remove(test_filename);
}

TEST_CASE("microtar error handling") {
    SUBCASE("error messages") {
        CHECK(std::string(mtar_strerror(MTAR_ESUCCESS)) == "success");
        CHECK(std::string(mtar_strerror(MTAR_ENOTFOUND)) == "file not found");
        CHECK(std::string(mtar_strerror(MTAR_EOPENFAIL)) == "could not open");
    }
    
    SUBCASE("file not found") {
        mtar_t tar;
        CHECK(mtar_open(&tar, "/nonexistent/path/file.tar", "r") == MTAR_EOPENFAIL);
    }
    
    SUBCASE("find nonexistent file") {
        const char* test_filename = "/tmp/test_microtar_error.tar";
        mtar_t tar;
        
        // Create archive with one file
        CHECK(mtar_open(&tar, test_filename, "w") == MTAR_ESUCCESS);
        std::string data = "test";
        CHECK(mtar_write_file_header(&tar, "existing.txt", data.size()) == MTAR_ESUCCESS);
        CHECK(mtar_write_data(&tar, data.c_str(), data.size()) == MTAR_ESUCCESS);
        CHECK(mtar_finalize(&tar) == MTAR_ESUCCESS);
        CHECK(mtar_close(&tar) == MTAR_ESUCCESS);
        
        // Try to find nonexistent file
        CHECK(mtar_open(&tar, test_filename, "r") == MTAR_ESUCCESS);
        mtar_header_t header;
        CHECK(mtar_find(&tar, "nonexistent.txt", &header) == MTAR_ENOTFOUND);
        CHECK(mtar_close(&tar) == MTAR_ESUCCESS);
        
        std::filesystem::remove(test_filename);
    }
}

TEST_CASE("microtar multiple files write") {
    const char* test_filename = "/tmp/test_microtar_multi_write.tar";
    mtar_t tar;
    
    CHECK(mtar_open(&tar, test_filename, "w") == MTAR_ESUCCESS);
    
    // Add first file
    std::string data1 = "First file content";
    CHECK(mtar_write_file_header(&tar, "file1.txt", data1.size()) == MTAR_ESUCCESS);
    CHECK(mtar_write_data(&tar, data1.c_str(), data1.size()) == MTAR_ESUCCESS);
    
    // Add second file
    std::string data2 = "Second file with different content";
    CHECK(mtar_write_file_header(&tar, "file2.txt", data2.size()) == MTAR_ESUCCESS);
    CHECK(mtar_write_data(&tar, data2.c_str(), data2.size()) == MTAR_ESUCCESS);
    
    CHECK(mtar_finalize(&tar) == MTAR_ESUCCESS);
    CHECK(mtar_close(&tar) == MTAR_ESUCCESS);
    
    std::filesystem::remove(test_filename);
}

TEST_CASE("microtar multiple files read") {
    const char* test_filename = "/tmp/test_microtar_multi_read.tar";
    mtar_t tar;
    
    // First create an archive with multiple files
    CHECK(mtar_open(&tar, test_filename, "w") == MTAR_ESUCCESS);
    std::string data1 = "First file content";
    CHECK(mtar_write_file_header(&tar, "file1.txt", data1.size()) == MTAR_ESUCCESS);
    CHECK(mtar_write_data(&tar, data1.c_str(), data1.size()) == MTAR_ESUCCESS);
    std::string data2 = "Second file with different content";
    CHECK(mtar_write_file_header(&tar, "file2.txt", data2.size()) == MTAR_ESUCCESS);
    CHECK(mtar_write_data(&tar, data2.c_str(), data2.size()) == MTAR_ESUCCESS);
    CHECK(mtar_finalize(&tar) == MTAR_ESUCCESS);
    CHECK(mtar_close(&tar) == MTAR_ESUCCESS);
    
    // Now test reading multiple files
    CHECK(mtar_open(&tar, test_filename, "r") == MTAR_ESUCCESS);
    
    // Read first file
    mtar_header_t header1{};
    CHECK(mtar_find(&tar, "file1.txt", &header1) == MTAR_ESUCCESS);
    std::string read_data1(header1.size, '\0');
    CHECK(mtar_read_data(&tar, &read_data1[0], header1.size) == MTAR_ESUCCESS);
    CHECK(read_data1 == "First file content");

    // Read second file
    mtar_header_t header2{};
    CHECK(mtar_find(&tar, "file2.txt", &header2) == MTAR_ESUCCESS);
    std::string read_data2(header2.size, '\0');
    CHECK(mtar_read_data(&tar, &read_data2[0], header2.size) == MTAR_ESUCCESS);
    CHECK(read_data2 == "Second file with different content");
    
    CHECK(mtar_close(&tar) == MTAR_ESUCCESS);
    
    std::filesystem::remove(test_filename);
}