#ifndef ZONEOUT_H
#define ZONEOUT_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Tolerance used by `sync::timestamps_close` — mirrors the C++
 * `sync_utils::CLOCK_SKEW_TOLERANCE` (5 seconds).
 */
#define CLOCK_SKEW_TOLERANCE_MS 5000

typedef struct ZoneoutZone ZoneoutZone;

typedef struct {
  double x;
  double y;
  double z;
} ZoneoutPoint3;

typedef struct {
  const ZoneoutPoint3 *ptr;
  uintptr_t len;
} ZoneoutPointArrayView;

typedef struct {
  double latitude;
  double longitude;
  double altitude;
} ZoneoutGeo3;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Human-readable zoneout version (e.g. `"0.1.0"`). Borrowed static.
 */
const char *zoneout_version(void);

/**
 * Retrieve the last error message set by a failing FFI call on the current
 * thread. Returns `NULL` when no error is pending.
 */
const char *zoneout_last_error_message(void);

void zoneout_string_free(char *ptr);

ZoneoutZone *zoneout_zone_new(const char *name,
                              const char *kind,
                              ZoneoutPointArrayView boundary,
                              ZoneoutGeo3 datum,
                              double resolution);

void zoneout_zone_free(ZoneoutZone *handle);

char *zoneout_zone_name(const ZoneoutZone *handle);

char *zoneout_zone_kind(const ZoneoutZone *handle);

uintptr_t zoneout_zone_child_count(const ZoneoutZone *handle);

double zoneout_zone_area(const ZoneoutZone *handle);

double zoneout_zone_perimeter(const ZoneoutZone *handle);

bool zoneout_zone_contains(const ZoneoutZone *handle, ZoneoutPoint3 point);

bool zoneout_zone_set_property(ZoneoutZone *handle, const char *key, const char *value);

char *zoneout_zone_get_property(const ZoneoutZone *handle, const char *key);

bool zoneout_zone_add_child(ZoneoutZone *parent, const ZoneoutZone *child);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* ZONEOUT_H */
