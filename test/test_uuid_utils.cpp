#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <unordered_set>
#include <unordered_map>
#include <set>
#include <vector>
#include <thread>
#include <mutex>

#include "zoneout/zoneout/uuid_utils.hpp"

using namespace zoneout;

TEST_CASE("UUID basic functionality") {
    SUBCASE("UUID generation") {
        UUID uuid1;
        UUID uuid2;
        
        // Two generated UUIDs should be different
        CHECK(uuid1 != uuid2);
        
        // UUIDs should not be null
        CHECK(!uuid1.isNull());
        CHECK(!uuid2.isNull());
    }
    
    SUBCASE("UUID string conversion") {
        UUID uuid;
        std::string str = uuid.toString();
        
        // Check format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        CHECK(str.length() == 36);
        CHECK(str[8] == '-');
        CHECK(str[13] == '-');
        CHECK(str[18] == '-');
        CHECK(str[23] == '-');
        
        // Round trip conversion
        UUID uuid2(str);
        CHECK(uuid == uuid2);
        CHECK(uuid.toString() == uuid2.toString());
    }
    
    SUBCASE("NULL UUID") {
        UUID null_uuid = UUID::null();
        CHECK(null_uuid.isNull());
        
        UUID generated;
        CHECK(!generated.isNull());
        CHECK(null_uuid != generated);
    }
    
    SUBCASE("UUID comparison operators") {
        UUID uuid1;
        UUID uuid2;
        UUID uuid1_copy(uuid1.toString());
        
        // Equality
        CHECK(uuid1 == uuid1_copy);
        CHECK(!(uuid1 == uuid2));
        
        // Inequality
        CHECK(uuid1 != uuid2);
        CHECK(!(uuid1 != uuid1_copy));
        
        // Less than (for ordering)
        std::set<UUID> uuid_set;
        uuid_set.insert(uuid1);
        uuid_set.insert(uuid2);
        CHECK(uuid_set.size() == 2);
    }
}

TEST_CASE("UUID container usage") {
    SUBCASE("unordered_set with UUID") {
        std::unordered_set<UUID, UUIDHash> uuid_set;
        
        UUID uuid1;
        UUID uuid2;
        
        uuid_set.insert(uuid1);
        uuid_set.insert(uuid2);
        uuid_set.insert(uuid1); // Duplicate
        
        CHECK(uuid_set.size() == 2);
        CHECK(uuid_set.find(uuid1) != uuid_set.end());
        CHECK(uuid_set.find(uuid2) != uuid_set.end());
    }
    
    SUBCASE("unordered_map with UUID") {
        std::unordered_map<UUID, std::string, UUIDHash> uuid_map;
        
        UUID uuid1;
        UUID uuid2;
        
        uuid_map[uuid1] = "first";
        uuid_map[uuid2] = "second";
        
        CHECK(uuid_map.size() == 2);
        CHECK(uuid_map[uuid1] == "first");
        CHECK(uuid_map[uuid2] == "second");
    }
}

TEST_CASE("UUID convenience functions") {
    SUBCASE("generateUUID function") {
        UUID uuid1 = generateUUID();
        UUID uuid2 = generateUUID();
        
        CHECK(uuid1 != uuid2);
        CHECK(!uuid1.isNull());
        CHECK(!uuid2.isNull());
    }
    
    SUBCASE("string conversion functions") {
        UUID original;
        std::string str = uuidToString(original);
        UUID converted = uuidFromString(str);
        
        CHECK(original == converted);
        CHECK(uuidToString(converted) == str);
    }
}

TEST_CASE("UUID error handling") {
    SUBCASE("invalid string format") {
        CHECK_THROWS_AS(UUID("invalid"), std::invalid_argument);
        CHECK_THROWS_AS(UUID("too-short"), std::invalid_argument);
        CHECK_THROWS_AS(UUID("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx-too-long"), std::invalid_argument);
        CHECK_THROWS_AS(uuidFromString("invalid"), std::invalid_argument);
    }
    
    SUBCASE("valid format variations") {
        // Test with valid hex characters
        CHECK_NOTHROW(UUID("12345678-1234-4567-8901-123456789abc"));
        CHECK_NOTHROW(UUID("ABCDEF12-ABCD-4EF1-ABCD-EF123456789A"));
    }
}

TEST_CASE("UUID version and variant") {
    SUBCASE("version 4 UUID") {
        for (int i = 0; i < 100; ++i) {
            UUID uuid;
            std::string str = uuid.toString();
            
            // Check version bit (position 14, should be '4')
            CHECK(str[14] == '4');
            
            // Check variant bits (position 19, should be '8', '9', 'a', 'b', 'A', or 'B')
            char variant = str[19];
            CHECK((variant == '8' || variant == '9' || 
                   variant == 'a' || variant == 'b' || 
                   variant == 'A' || variant == 'B'));
        }
    }
}

TEST_CASE("UUID thread safety") {
    SUBCASE("concurrent generation") {
        std::set<UUID> generated_uuids;
        const int num_threads = 4;
        const int uuids_per_thread = 100;
        
        std::vector<std::thread> threads;
        std::mutex mutex;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                std::vector<UUID> local_uuids;
                for (int i = 0; i < uuids_per_thread; ++i) {
                    local_uuids.push_back(generateUUID());
                }
                
                std::lock_guard<std::mutex> lock(mutex);
                for (const auto& uuid : local_uuids) {
                    generated_uuids.insert(uuid);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        // All UUIDs should be unique
        CHECK(generated_uuids.size() == num_threads * uuids_per_thread);
    }
}