use std::ffi::CStr;
use std::os::raw::{c_char, c_double, c_long, c_uint, c_ushort};
use std::time::{SystemTime, UNIX_EPOCH};

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
struct CGEventTypeC(u32); //TODO: enum

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
struct CGPointC {
    x: c_double,
    y: c_double,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
struct TimespecC {
    tv_sec: c_long,
    tv_nsec: c_long,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ActionEventC {
    time_down: c_long,
    r#type: CGEventTypeC,
    key_code: c_ushort,
    normalised_click_point: CGPointC,
    dragged_distance: c_uint,
    is_builtin_display: bool,
    is_main_display: bool,
    process_name: *const c_char,
    function_start: TimespecC,
}
impl ActionEventC {
    pub fn tidy_up(&self) -> ActionEvent {
        // Get difference between now and function start
        let now = SystemTime::now().duration_since(UNIX_EPOCH).unwrap();
        let diff_sec_ex = now.as_secs() as i64 - self.function_start.tv_sec;
        let diff_nsec_ex = now.subsec_nanos() as i64 - self.function_start.tv_nsec;
        let diff_us = diff_sec_ex * 1_000_000 + diff_nsec_ex / 1_000;

        let process_name_cstr = unsafe { CStr::from_ptr(self.process_name) };
        let process_name = String::from_utf8_lossy(process_name_cstr.to_bytes()).to_string();

        ActionEvent {
            time_down_ms: self.time_down as i32,
            r#type: self.r#type.0,
            key_code: self.key_code,
            normalised_click_point: (
                self.normalised_click_point.x as f32,
                self.normalised_click_point.y as f32,
            ),
            dragged_distance_px: self.dragged_distance,
            is_builtin_display: self.is_builtin_display,
            is_main_display: self.is_main_display,
            process_name,
            execution_time_us: diff_us as i32,
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct ActionEvent {
    pub time_down_ms: i32,
    pub r#type: u32,
    pub key_code: u16,
    pub normalised_click_point: (f32, f32),
    pub dragged_distance_px: u32,
    pub is_builtin_display: bool,
    pub is_main_display: bool,
    pub process_name: String,
    pub execution_time_us: i32,
}
