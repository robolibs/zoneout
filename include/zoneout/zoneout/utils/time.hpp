#pragma once

#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

namespace zoneout {

    // Type aliases for consistent time handling
    using Timestamp = std::chrono::system_clock::time_point;
    using Duration = std::chrono::milliseconds;
    using Clock = std::chrono::system_clock;

    // Lamport logical clock for distributed coordination
    class LamportClock {
      public:
        LamportClock() : counter_(0) {}

        explicit LamportClock(uint64_t initial_value) : counter_(initial_value) {}

        // Get current logical time and increment
        uint64_t tick() { return ++counter_; }

        // Update clock with received timestamp (for distributed coordination)
        uint64_t update(uint64_t received_time) {
            uint64_t current = counter_.load();
            uint64_t new_time = std::max(current, received_time) + 1;
            counter_.store(new_time);
            return new_time;
        }

        // Get current logical time without incrementing
        uint64_t time() const { return counter_.load(); }

        // Reset clock
        void reset(uint64_t value = 0) { counter_.store(value); }

      private:
        std::atomic<uint64_t> counter_;
    };

    // Time utility functions
    namespace time_utils {

        // Get current timestamp
        inline Timestamp now() { return Clock::now(); }

        // Convert timestamp to ISO 8601 string (UTC)
        inline std::string toISO8601(const Timestamp &timestamp) {
            auto time_t = Clock::to_time_t(timestamp);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()) % 1000;

            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
            ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
            return ss.str();
        }

        // Parse ISO 8601 string to timestamp
        inline Timestamp fromISO8601(const std::string &iso_string) {
            std::tm tm = {};
            std::istringstream ss(iso_string);

            // Parse the date and time part
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

            if (ss.fail()) {
                throw std::invalid_argument("Invalid ISO 8601 format");
            }

            auto time_point = Clock::from_time_t(std::mktime(&tm));

            // Parse milliseconds if present
            if (ss.peek() == '.') {
                ss.ignore(); // skip '.'
                int ms;
                ss >> ms;
                if (!ss.fail()) {
                    time_point += std::chrono::milliseconds(ms);
                }
            }

            return time_point;
        }

        // Convert timestamp to milliseconds since epoch
        inline uint64_t toMilliseconds(const Timestamp &timestamp) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
        }

        // Create timestamp from milliseconds since epoch
        inline Timestamp fromMilliseconds(uint64_t ms) { return Timestamp(std::chrono::milliseconds(ms)); }

        // Duration helpers
        inline Duration seconds(double s) {
            return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(s));
        }

        inline Duration minutes(double m) {
            return std::chrono::duration_cast<Duration>(std::chrono::duration<double, std::ratio<60>>(m));
        }

        inline Duration hours(double h) {
            return std::chrono::duration_cast<Duration>(std::chrono::duration<double, std::ratio<3600>>(h));
        }

        // Duration to string
        inline std::string durationToString(const Duration &duration) {
            auto total_ms = duration.count();
            auto hours = total_ms / (1000 * 60 * 60);
            auto minutes = (total_ms % (1000 * 60 * 60)) / (1000 * 60);
            auto seconds = (total_ms % (1000 * 60)) / 1000;
            auto ms = total_ms % 1000;

            std::stringstream ss;
            if (hours > 0) {
                ss << hours << "h ";
            }
            if (minutes > 0) {
                ss << minutes << "m ";
            }
            if (seconds > 0) {
                ss << seconds << "s ";
            }
            if (ms > 0 || total_ms == 0) {
                ss << ms << "ms";
            }

            std::string result = ss.str();
            if (!result.empty() && result.back() == ' ') {
                result.pop_back();
            }
            return result;
        }

        // Check if timestamp is in the future
        inline bool isFuture(const Timestamp &timestamp) { return timestamp > now(); }

        // Check if timestamp is in the past
        inline bool isPast(const Timestamp &timestamp) { return timestamp < now(); }

        // Check if duration has elapsed since timestamp
        inline bool hasElapsed(const Timestamp &start, const Duration &duration) { return (now() - start) >= duration; }

        // Get time remaining until timestamp
        inline Duration timeUntil(const Timestamp &future_time) {
            auto remaining = future_time - now();
            return remaining > Duration::zero() ? std::chrono::duration_cast<Duration>(remaining) : Duration::zero();
        }

        // Get time elapsed since timestamp
        inline Duration timeSince(const Timestamp &past_time) {
            return std::chrono::duration_cast<Duration>(now() - past_time);
        }

        // Add duration to timestamp
        inline Timestamp add(const Timestamp &timestamp, const Duration &duration) { return timestamp + duration; }

        // Subtract duration from timestamp
        inline Timestamp subtract(const Timestamp &timestamp, const Duration &duration) { return timestamp - duration; }

    } // namespace time_utils

    // Clock synchronization utilities for distributed systems
    namespace sync_utils {

        // Clock skew tolerance (for distributed systems)
        constexpr Duration CLOCK_SKEW_TOLERANCE = Duration(5000); // 5 seconds

        // Check if two timestamps are "close enough" considering clock skew
        inline bool areTimestampsClose(const Timestamp &t1, const Timestamp &t2,
                                       const Duration &tolerance = CLOCK_SKEW_TOLERANCE) {
            auto diff = t1 > t2 ? t1 - t2 : t2 - t1;
            return std::chrono::duration_cast<Duration>(diff) <= tolerance;
        }

        // Estimate clock offset between local and remote time
        inline Duration estimateClockOffset(const Timestamp &local_send_time, const Timestamp &remote_time,
                                            const Timestamp &local_receive_time) {
            auto round_trip = local_receive_time - local_send_time;
            auto estimated_remote_receive = local_send_time + round_trip / 2;
            return std::chrono::duration_cast<Duration>(remote_time - estimated_remote_receive);
        }

    } // namespace sync_utils

} // namespace zoneout