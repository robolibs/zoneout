//! C ABI for zoneout.
//!
//! Conventions: opaque Box-backed handles (free with the matching
//! *_free); fallible calls return bool/int with the reason in the
//! thread-local zoneout_last_error_message(); borrowed views are valid
//! only for the lifetime documented by the handle they came from.
//!
//! `include/zoneout.h` is generated from this file by cbindgen.

// extern "C" fns take raw pointers from C and deref them by design.
#![allow(clippy::not_unsafe_ptr_arg_deref)]

use std::cell::RefCell;
use std::ffi::{CStr, CString, c_char};
use std::ptr;

use datapod::{Geo, Point, Polygon};

use crate::Zone;

thread_local! {
    static LAST_ERROR: RefCell<Option<CString>> = const { RefCell::new(None) };
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ZoneoutGeo3 {
    pub latitude: f64,
    pub longitude: f64,
    pub altitude: f64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ZoneoutPoint3 {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ZoneoutPointArrayView {
    pub ptr: *const ZoneoutPoint3,
    pub len: usize,
}

pub struct ZoneoutZone {
    inner: Zone,
}

impl From<ZoneoutGeo3> for Geo {
    fn from(value: ZoneoutGeo3) -> Self {
        Self::new(value.latitude, value.longitude, value.altitude)
    }
}

impl From<Geo> for ZoneoutGeo3 {
    fn from(value: Geo) -> Self {
        Self {
            latitude: value.latitude,
            longitude: value.longitude,
            altitude: value.altitude,
        }
    }
}

impl From<ZoneoutPoint3> for Point {
    fn from(value: ZoneoutPoint3) -> Self {
        Self::new(value.x, value.y, value.z)
    }
}

impl From<Point> for ZoneoutPoint3 {
    fn from(value: Point) -> Self {
        Self {
            x: value.x,
            y: value.y,
            z: value.z,
        }
    }
}

fn clear_last_error() {
    LAST_ERROR.with(|slot| *slot.borrow_mut() = None);
}

fn set_last_error(message: impl Into<String>) {
    let message = message.into().replace('\0', " ");
    LAST_ERROR.with(|slot| {
        *slot.borrow_mut() = Some(
            CString::new(message).unwrap_or_else(|_| CString::new("zoneout ffi error").unwrap()),
        );
    });
}

unsafe fn cstr<'a>(value: *const c_char, label: &str) -> Result<&'a str, ()> {
    if value.is_null() {
        set_last_error(format!("null {label} pointer"));
        return Err(());
    }
    // SAFETY: caller provided a non-null, NUL-terminated C string.
    unsafe { CStr::from_ptr(value) }.to_str().map_err(|_| {
        set_last_error(format!("{label} must be valid UTF-8"));
    })
}

fn points_from_view(view: ZoneoutPointArrayView) -> Result<Vec<Point>, ()> {
    if view.ptr.is_null() && view.len != 0 {
        set_last_error("null point array pointer");
        return Err(());
    }
    if view.len < 3 {
        set_last_error("zone boundary requires at least three points");
        return Err(());
    }
    // SAFETY: pointer is either null with len 0 or points to len ZoneoutPoint3 values.
    let points = unsafe { std::slice::from_raw_parts(view.ptr, view.len) };
    Ok(points.iter().copied().map(Point::from).collect())
}

fn zone_from_ptr_mut<'a>(handle: *mut ZoneoutZone) -> Result<&'a mut ZoneoutZone, ()> {
    if handle.is_null() {
        set_last_error("null zone handle");
        return Err(());
    }
    // SAFETY: non-null pointer originated from Box::into_raw in this ABI.
    Ok(unsafe { &mut *handle })
}

fn zone_from_ptr<'a>(handle: *const ZoneoutZone) -> Result<&'a ZoneoutZone, ()> {
    if handle.is_null() {
        set_last_error("null zone handle");
        return Err(());
    }
    // SAFETY: non-null pointer originated from Box::into_raw in this ABI.
    Ok(unsafe { &*handle })
}

fn string_to_ptr(value: impl AsRef<str>) -> *mut c_char {
    CString::new(value.as_ref())
        .unwrap_or_else(|_| CString::new("").unwrap())
        .into_raw()
}

/// Human-readable zoneout version (e.g. `"0.1.0"`). Borrowed static.
#[unsafe(no_mangle)]
pub extern "C" fn zoneout_version() -> *const c_char {
    static VERSION: &str = concat!(env!("CARGO_PKG_VERSION"), "\0");
    VERSION.as_ptr().cast()
}

/// Retrieve the last error message set by a failing FFI call on the current
/// thread. Returns `NULL` when no error is pending.
#[unsafe(no_mangle)]
pub extern "C" fn zoneout_last_error_message() -> *const c_char {
    LAST_ERROR.with(|slot| {
        slot.borrow()
            .as_ref()
            .map_or(ptr::null(), |message| message.as_ptr())
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_string_free(ptr: *mut c_char) {
    if !ptr.is_null() {
        // SAFETY: pointer came from CString::into_raw in this ABI.
        unsafe { drop(CString::from_raw(ptr)) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_new(
    name: *const c_char,
    kind: *const c_char,
    boundary: ZoneoutPointArrayView,
    datum: ZoneoutGeo3,
    resolution: f64,
) -> *mut ZoneoutZone {
    clear_last_error();
    let result: Result<Zone, ()> = (|| {
        // SAFETY: cstr validates null and UTF-8.
        let name = unsafe { cstr(name, "name") }?;
        // SAFETY: cstr validates null and UTF-8.
        let kind = unsafe { cstr(kind, "kind") }?;
        let polygon = Polygon::new(points_from_view(boundary)?);
        Zone::new(name, kind, polygon, datum.into(), resolution).map_err(|err| {
            set_last_error(err.to_string());
        })
    })();
    match result {
        Ok(inner) => Box::into_raw(Box::new(ZoneoutZone { inner })),
        Err(()) => ptr::null_mut(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_free(handle: *mut ZoneoutZone) {
    if !handle.is_null() {
        // SAFETY: pointer came from Box::into_raw in this ABI.
        unsafe { drop(Box::from_raw(handle)) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_name(handle: *const ZoneoutZone) -> *mut c_char {
    clear_last_error();
    match zone_from_ptr(handle) {
        Ok(zone) => string_to_ptr(zone.inner.name()),
        Err(()) => ptr::null_mut(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_kind(handle: *const ZoneoutZone) -> *mut c_char {
    clear_last_error();
    match zone_from_ptr(handle) {
        Ok(zone) => string_to_ptr(zone.inner.kind()),
        Err(()) => ptr::null_mut(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_child_count(handle: *const ZoneoutZone) -> usize {
    clear_last_error();
    match zone_from_ptr(handle) {
        Ok(zone) => zone.inner.child_count(),
        Err(()) => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_area(handle: *const ZoneoutZone) -> f64 {
    clear_last_error();
    match zone_from_ptr(handle) {
        Ok(zone) => zone.inner.poly().area(),
        Err(()) => f64::NAN,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_perimeter(handle: *const ZoneoutZone) -> f64 {
    clear_last_error();
    match zone_from_ptr(handle) {
        Ok(zone) => zone.inner.poly().perimeter(),
        Err(()) => f64::NAN,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_contains(handle: *const ZoneoutZone, point: ZoneoutPoint3) -> bool {
    clear_last_error();
    match zone_from_ptr(handle) {
        Ok(zone) => zone.inner.contains(point.into()),
        Err(()) => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_set_property(
    handle: *mut ZoneoutZone,
    key: *const c_char,
    value: *const c_char,
) -> bool {
    clear_last_error();
    let result: Result<(), ()> = (|| {
        let zone = zone_from_ptr_mut(handle)?;
        // SAFETY: cstr validates null and UTF-8.
        let key = unsafe { cstr(key, "key") }?;
        // SAFETY: cstr validates null and UTF-8.
        let value = unsafe { cstr(value, "value") }?;
        zone.inner.set_property(key, value);
        Ok(())
    })();
    result.is_ok()
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_get_property(
    handle: *const ZoneoutZone,
    key: *const c_char,
) -> *mut c_char {
    clear_last_error();
    let result = (|| {
        let zone = zone_from_ptr(handle)?;
        // SAFETY: cstr validates null and UTF-8.
        let key = unsafe { cstr(key, "key") }?;
        zone.inner
            .property(key)
            .cloned()
            .ok_or_else(|| set_last_error(format!("property '{key}' not found")))
    })();
    match result {
        Ok(value) => string_to_ptr(value),
        Err(()) => ptr::null_mut(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn zoneout_zone_add_child(
    parent: *mut ZoneoutZone,
    child: *const ZoneoutZone,
) -> bool {
    clear_last_error();
    let result = (|| {
        let parent = zone_from_ptr_mut(parent)?;
        let child = zone_from_ptr(child)?;
        parent.inner.add_child(child.inner.clone()).map_err(|err| {
            set_last_error(err.to_string());
        })
    })();
    result.is_ok()
}
