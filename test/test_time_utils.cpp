#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <thread>
#include <chrono>

#include "zoneout/zoneout/time_utils.hpp"

using namespace zoneout;
using namespace zoneout::time_utils;

TEST_CASE("Time utilities basic functionality") {
    SUBCASE("Current time") {
        auto t1 = now();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto t2 = now();
        
        CHECK(t2 > t1);
        CHECK(timeSince(t1).count() >= 10);
    }
    
    SUBCASE("Duration helpers") {
        auto s = seconds(1.5);
        auto m = minutes(1.0);
        auto h = hours(1.0);
        
        CHECK(s.count() == 1500);
        CHECK(m.count() == 60000);
        CHECK(h.count() == 3600000);
    }
    
    SUBCASE("Duration to string") {
        CHECK(durationToString(Duration(0)) == "0ms");
        CHECK(durationToString(Duration(500)) == "500ms");
        CHECK(durationToString(Duration(1500)) == "1s 500ms");
        CHECK(durationToString(Duration(65000)) == "1m 5s");
        CHECK(durationToString(Duration(3665000)) == "1h 1m 5s");
    }
}

TEST_CASE("ISO 8601 string conversion") {
    SUBCASE("Round trip conversion") {
        auto original = now();
        std::string iso_str = toISO8601(original);
        
        // Check format (basic structure)
        CHECK(iso_str.length() >= 20); // YYYY-MM-DDTHH:MM:SS.sssZ
        CHECK(iso_str.find('T') != std::string::npos);
        CHECK(iso_str.back() == 'Z');
        
        // Round trip should be close (within 1ms due to precision)
        auto parsed = fromISO8601(iso_str);
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            original > parsed ? original - parsed : parsed - original);
        CHECK(diff.count() <= 1);
    }
    
    SUBCASE("Parse specific format") {
        std::string test_iso = "2024-03-15T14:30:45.123Z";
        auto timestamp = fromISO8601(test_iso);
        
        // Convert back and check
        std::string back_to_iso = toISO8601(timestamp);
        CHECK(back_to_iso == test_iso);
    }
    
    SUBCASE("Invalid format handling") {
        CHECK_THROWS_AS(fromISO8601("invalid"), std::invalid_argument);
        CHECK_THROWS_AS(fromISO8601("2024-13-45"), std::invalid_argument);
    }
}

TEST_CASE("Timestamp utilities") {
    auto base_time = now();
    
    SUBCASE("Future and past detection") {
        auto future_time = add(base_time, minutes(5));
        auto past_time = subtract(base_time, minutes(5));
        
        CHECK(isFuture(future_time));
        CHECK(!isFuture(past_time));
        CHECK(isPast(past_time));
        CHECK(!isPast(future_time));
    }
    
    SUBCASE("Time calculations") {
        auto future_time = add(base_time, seconds(30));
        
        // Time until future
        auto until = timeUntil(future_time);
        CHECK(until.count() > 0);
        CHECK(until.count() <= 30000); // Should be <= 30 seconds
        
        // Time since past
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto since = timeSince(base_time);
        CHECK(since.count() >= 50);
    }
    
    SUBCASE("Elapsed time checking") {
        auto start = now();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        CHECK(hasElapsed(start, Duration(50)));
        CHECK(!hasElapsed(start, Duration(200)));
    }
}

TEST_CASE("Milliseconds conversion") {
    SUBCASE("To and from milliseconds") {
        auto timestamp = now();
        uint64_t ms = toMilliseconds(timestamp);
        auto back = fromMilliseconds(ms);
        
        // Should be exactly the same (millisecond precision)
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp > back ? timestamp - back : back - timestamp);
        CHECK(diff.count() == 0);
    }
    
    SUBCASE("Known values") {
        // Unix epoch
        auto epoch = fromMilliseconds(0);
        CHECK(toMilliseconds(epoch) == 0);
        
        // Known timestamp: 2024-01-01 00:00:00 UTC = 1704067200000 ms
        auto new_year_2024 = fromMilliseconds(1704067200000);
        CHECK(toMilliseconds(new_year_2024) == 1704067200000);
    }
}

TEST_CASE("Lamport clock functionality") {
    SUBCASE("Basic operations") {
        LamportClock clock;
        
        CHECK(clock.time() == 0);
        
        auto t1 = clock.tick();
        CHECK(t1 == 1);
        CHECK(clock.time() == 1);
        
        auto t2 = clock.tick();
        CHECK(t2 == 2);
        CHECK(clock.time() == 2);
    }
    
    SUBCASE("Clock synchronization") {
        LamportClock clock1;
        LamportClock clock2;
        
        // Clock 1 advances
        clock1.tick(); // 1
        clock1.tick(); // 2
        clock1.tick(); // 3
        
        // Clock 2 receives message from clock 1
        auto received_time = clock1.time();
        auto new_time = clock2.update(received_time);
        
        CHECK(new_time == 4); // max(0, 3) + 1
        CHECK(clock2.time() == 4);
        
        // Clock 2 advances further
        clock2.tick(); // 5
        
        // Clock 1 receives message from clock 2
        auto new_time1 = clock1.update(clock2.time());
        CHECK(new_time1 == 6); // max(3, 5) + 1
    }
    
    SUBCASE("Reset functionality") {
        LamportClock clock;
        clock.tick();
        clock.tick();
        CHECK(clock.time() == 2);
        
        clock.reset();
        CHECK(clock.time() == 0);
        
        clock.reset(10);
        CHECK(clock.time() == 10);
    }
    
    SUBCASE("Concurrent access") {
        LamportClock clock;
        const int num_threads = 4;
        const int ticks_per_thread = 100;
        
        std::vector<std::thread> threads;
        std::vector<uint64_t> results(num_threads * ticks_per_thread);
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < ticks_per_thread; ++i) {
                    results[t * ticks_per_thread + i] = clock.tick();
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        // All results should be unique and positive
        std::set<uint64_t> unique_results(results.begin(), results.end());
        CHECK(unique_results.size() == results.size());
        CHECK(*unique_results.begin() > 0);
        CHECK(*unique_results.rbegin() == num_threads * ticks_per_thread);
    }
}

TEST_CASE("Clock synchronization utilities") {
    using namespace sync_utils;
    
    SUBCASE("Timestamp proximity") {
        auto t1 = now();
        auto t2 = add(t1, Duration(1000)); // 1 second later
        auto t3 = add(t1, Duration(10000)); // 10 seconds later
        
        CHECK(areTimestampsClose(t1, t2, Duration(2000))); // Within 2 seconds
        CHECK(!areTimestampsClose(t1, t3, Duration(2000))); // Beyond 2 seconds
        
        // Default tolerance (5 seconds)
        CHECK(areTimestampsClose(t1, t2));
        CHECK(!areTimestampsClose(t1, t3));
    }
    
    SUBCASE("Clock offset estimation") {
        auto local_send = now();
        auto remote_time = add(local_send, Duration(2000)); // Remote is 2s ahead
        auto local_receive = add(local_send, Duration(100)); // 100ms round trip
        
        auto offset = estimateClockOffset(local_send, remote_time, local_receive);
        
        // Should estimate approximately 1950ms offset (2000 - 50ms/2 for round trip)
        CHECK(offset.count() >= 1900);
        CHECK(offset.count() <= 2000);
    }
}