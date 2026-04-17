//! Lamport clock + ISO-8601 timestamp helpers.
//!
//! Ported from `include/zoneout/zoneout/utils/time.hpp`. The port keeps the
//! same public shape (tick / observe / current / merge) but uses an
//! `AtomicU64` instead of the C++ `std::atomic<uint64_t>`.

use std::sync::atomic::{AtomicU64, Ordering};

use chrono::{DateTime, Duration as ChronoDuration, Utc};
use serde::{Deserialize, Serialize};

/// Tolerance used by `sync::timestamps_close` — mirrors the C++
/// `sync_utils::CLOCK_SKEW_TOLERANCE` (5 seconds).
pub const CLOCK_SKEW_TOLERANCE_MS: i64 = 5_000;

#[derive(Debug, Default)]
pub struct LamportClock {
    counter: AtomicU64,
}

impl LamportClock {
    pub const fn new() -> Self {
        Self { counter: AtomicU64::new(0) }
    }

    /// Increment and return the new value (local event).
    pub fn tick(&self) -> u64 {
        self.counter.fetch_add(1, Ordering::AcqRel) + 1
    }

    /// Observe a remote timestamp; advance local to `max(local, remote) + 1`.
    pub fn observe(&self, remote: u64) -> u64 {
        let mut cur = self.counter.load(Ordering::Acquire);
        loop {
            let next = cur.max(remote) + 1;
            match self.counter.compare_exchange_weak(
                cur,
                next,
                Ordering::AcqRel,
                Ordering::Acquire,
            ) {
                Ok(_) => return next,
                Err(v) => cur = v,
            }
        }
    }

    pub fn current(&self) -> u64 {
        self.counter.load(Ordering::Acquire)
    }

    pub fn reset(&self) {
        self.counter.store(0, Ordering::Release);
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct Timestamp(#[serde(with = "chrono::serde::ts_milliseconds")] pub DateTime<Utc>);

impl Timestamp {
    pub fn now() -> Self {
        Self(Utc::now())
    }

    pub fn from_millis(ms: i64) -> Self {
        Self(DateTime::<Utc>::from_timestamp_millis(ms).unwrap_or_else(Utc::now))
    }

    pub fn to_iso8601(&self) -> String {
        self.0.to_rfc3339()
    }

    pub fn parse_iso8601(s: &str) -> Option<Self> {
        DateTime::parse_from_rfc3339(s).ok().map(|dt| Self(dt.with_timezone(&Utc)))
    }

    pub fn elapsed_ms(&self) -> i64 {
        Utc::now().signed_duration_since(self.0).num_milliseconds()
    }

    pub fn to_millis(&self) -> i64 { self.0.timestamp_millis() }
    pub fn is_future(&self) -> bool { self.0 > Utc::now() }
    pub fn is_past(&self) -> bool { self.0 < Utc::now() }
}

// -- duration helpers ------------------------------------------------------

/// `Duration` mirrors the C++ `time_utils::Duration` helpers by wrapping
/// `chrono::Duration` with clearer constructors.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Duration(pub ChronoDuration);

impl Duration {
    pub fn zero() -> Self { Self(ChronoDuration::zero()) }

    pub fn from_millis(ms: i64) -> Self { Self(ChronoDuration::milliseconds(ms)) }
    pub fn from_seconds(seconds: f64) -> Self {
        Self(ChronoDuration::milliseconds((seconds * 1000.0) as i64))
    }
    pub fn from_minutes(minutes: f64) -> Self { Self::from_seconds(minutes * 60.0) }
    pub fn from_hours(hours: f64)     -> Self { Self::from_seconds(hours   * 3600.0) }

    pub fn as_millis(self) -> i64 { self.0.num_milliseconds() }
    pub fn as_seconds(self) -> f64 { self.0.num_milliseconds() as f64 / 1000.0 }

    pub fn to_string_hms(self) -> String {
        let mut ms = self.0.num_milliseconds();
        let sign = if ms < 0 { ms = -ms; "-" } else { "" };
        let h =  ms / 3_600_000; ms %= 3_600_000;
        let m =  ms /    60_000; ms %=    60_000;
        let s =  ms /     1_000; ms %=     1_000;
        if h > 0 { format!("{sign}{h}h {m}m {s}s") }
        else if m > 0 { format!("{sign}{m}m {s}s") }
        else if s > 0 { format!("{sign}{s}s {ms}ms") }
        else { format!("{sign}{ms}ms") }
    }
}

pub fn add(t: Timestamp, d: Duration) -> Timestamp { Timestamp(t.0 + d.0) }
pub fn sub(t: Timestamp, d: Duration) -> Timestamp { Timestamp(t.0 - d.0) }

pub fn time_since(t: Timestamp) -> Duration {
    Duration(Utc::now().signed_duration_since(t.0))
}
pub fn time_until(t: Timestamp) -> Duration {
    Duration(t.0.signed_duration_since(Utc::now()))
}
pub fn has_elapsed(t: Timestamp, d: Duration) -> bool {
    time_since(t).0 >= d.0
}

// -- distributed sync helpers ---------------------------------------------

pub mod sync {
    use super::{CLOCK_SKEW_TOLERANCE_MS, Duration, Timestamp};

    pub fn timestamps_close(a: Timestamp, b: Timestamp) -> bool {
        timestamps_close_with(a, b, Duration::from_millis(CLOCK_SKEW_TOLERANCE_MS))
    }

    pub fn timestamps_close_with(a: Timestamp, b: Timestamp, tol: Duration) -> bool {
        (a.to_millis() - b.to_millis()).abs() <= tol.as_millis()
    }

    /// NTP-style one-way offset estimate:
    ///   offset ≈ ((local_send + local_recv) / 2) − remote
    pub fn estimate_clock_offset(
        local_send: Timestamp,
        remote: Timestamp,
        local_recv: Timestamp,
    ) -> Duration {
        let midpoint_ms = (local_send.to_millis() + local_recv.to_millis()) / 2;
        Duration::from_millis(midpoint_ms - remote.to_millis())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lamport_tick_monotonic() {
        let c = LamportClock::new();
        assert_eq!(c.tick(), 1);
        assert_eq!(c.tick(), 2);
        assert_eq!(c.current(), 2);
    }

    #[test]
    fn lamport_observe_advances() {
        let c = LamportClock::new();
        c.tick();
        assert_eq!(c.observe(10), 11);
        assert_eq!(c.observe(5), 12);
    }

    #[test]
    fn timestamp_roundtrip_iso8601() {
        let t = Timestamp::now();
        let s = t.to_iso8601();
        let parsed = Timestamp::parse_iso8601(&s).expect("parse");
        assert_eq!(t.0.timestamp_millis(), parsed.0.timestamp_millis());
    }

    #[test]
    fn duration_builders_and_hms() {
        assert_eq!(Duration::from_seconds(1.5).as_millis(), 1500);
        assert_eq!(Duration::from_minutes(2.0).as_millis(), 120_000);
        assert_eq!(Duration::from_hours(1.0).as_millis(), 3_600_000);
        let d = Duration::from_seconds(3661.0);
        assert_eq!(d.to_string_hms(), "1h 1m 1s");
    }

    #[test]
    fn time_since_and_elapsed() {
        let past = add(Timestamp::now(), Duration::from_seconds(-2.0));
        assert!(has_elapsed(past, Duration::from_seconds(1.0)));
        assert!(!has_elapsed(past, Duration::from_seconds(10.0)));
    }

    #[test]
    fn sync_timestamps_close() {
        // Default tolerance is 5 seconds.
        let a = Timestamp::from_millis(1_000);
        let b = Timestamp::from_millis(3_000);
        assert!(sync::timestamps_close(a, b));
        let c = Timestamp::from_millis(10_000);
        assert!(!sync::timestamps_close(a, c));
    }
}

