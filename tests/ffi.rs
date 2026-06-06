use std::ffi::{CStr, CString};

use zoneout::ffi::*;

fn square() -> [ZoneoutPoint3; 4] {
    [
        ZoneoutPoint3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        ZoneoutPoint3 {
            x: 10.0,
            y: 0.0,
            z: 0.0,
        },
        ZoneoutPoint3 {
            x: 10.0,
            y: 10.0,
            z: 0.0,
        },
        ZoneoutPoint3 {
            x: 0.0,
            y: 10.0,
            z: 0.0,
        },
    ]
}

#[test]
fn c_abi_zone_lifecycle_and_properties_work() {
    let name = CString::new("field").unwrap();
    let kind = CString::new("farm").unwrap();
    let boundary = square();
    let zone = zoneout_zone_new(
        name.as_ptr(),
        kind.as_ptr(),
        ZoneoutPointArrayView {
            ptr: boundary.as_ptr(),
            len: boundary.len(),
        },
        ZoneoutGeo3 {
            latitude: 1.0,
            longitude: 2.0,
            altitude: 3.0,
        },
        0.0,
    );
    assert!(!zone.is_null());
    assert_eq!(zoneout_zone_child_count(zone), 0);
    assert_eq!(zoneout_zone_area(zone), 100.0);
    assert!(zoneout_zone_contains(
        zone,
        ZoneoutPoint3 {
            x: 5.0,
            y: 5.0,
            z: 0.0
        }
    ));

    let key = CString::new("crop").unwrap();
    let value = CString::new("wheat").unwrap();
    assert!(zoneout_zone_set_property(
        zone,
        key.as_ptr(),
        value.as_ptr()
    ));
    let got = zoneout_zone_get_property(zone, key.as_ptr());
    assert!(!got.is_null());
    let got_text = unsafe { CStr::from_ptr(got) }.to_str().unwrap().to_string();
    assert_eq!(got_text, "wheat");
    zoneout_string_free(got);

    zoneout_zone_free(zone);
}

#[test]
fn c_abi_reports_invalid_input() {
    let name = CString::new("bad").unwrap();
    let kind = CString::new("farm").unwrap();
    let zone = zoneout_zone_new(
        name.as_ptr(),
        kind.as_ptr(),
        ZoneoutPointArrayView {
            ptr: std::ptr::null(),
            len: 2,
        },
        ZoneoutGeo3 {
            latitude: 0.0,
            longitude: 0.0,
            altitude: 0.0,
        },
        0.0,
    );
    assert!(zone.is_null());
    assert!(!zoneout_last_error_message().is_null());
}
