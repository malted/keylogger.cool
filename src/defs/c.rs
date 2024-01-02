// use crate::defs::event::{Event, EventType};
use super::event::{BaseEvent, Event, EventType};
use std::ffi::CStr;
use std::os::raw::{c_char, c_double, c_int, c_long, c_ushort};
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
    key_char: *const c_char,
    normalised_click_point: CGPointC,
    scroll_delta_x: c_int,
    scroll_delta_y: c_int,
    dragged_distance_px: c_double,
    dragged_distance_mm: c_double,
    is_builtin_display: bool,
    is_main_display: bool,
    process_name: *const c_char,
    function_start: TimespecC,
}
impl ActionEventC {
    pub fn tidy_up(&self) -> Result<Event, Box<dyn std::error::Error>> {
        let r#type = EventType::from_u32(self.r#type.0);

        let process_name_cstr = unsafe { CStr::from_ptr(self.process_name) };
        let process_name = String::from_utf8_lossy(process_name_cstr.to_bytes()).to_string();

        // Get difference between now and function start
        let now = SystemTime::now().duration_since(UNIX_EPOCH)?;
        let diff_sec_ex = now.as_secs() as i64 - self.function_start.tv_sec;
        let diff_nsec_ex = now.subsec_nanos() as i64 - self.function_start.tv_nsec;
        let diff_us = (diff_sec_ex * 1_000_000 + diff_nsec_ex / 1_000) as i32;

        let key_char_cstr = unsafe { CStr::from_ptr(self.key_char) };
        let key_char = match std::str::from_utf8(key_char_cstr.to_bytes()) {
            Ok(v) => v.to_string(),
            Err(_e) => {
                // Assume it's UTF-16
                let u16_bytes =
                    unsafe { std::slice::from_raw_parts(self.key_char as *const u16, 1) };

                String::from_utf16_lossy(u16_bytes)
            }
        };

        if self.time_down < 0 || self.time_down > u32::MAX as i64 {
            return Err("time_down is out of range".into());
        }

        let base = BaseEvent {
            r#type,
            is_builtin_display: self.is_builtin_display,
            is_main_display: self.is_main_display,
            process_name,
            execution_time_us: diff_us,
        };

        let event: Event = if r#type.is_keyboard_event() {
            Event::Keyboard {
                base,
                time_down_ms: self.time_down as u32,
                key_code: self.key_code,
                key_char,
            }
        } else if r#type.is_mouse_event() {
            Event::Mouse {
                base,
                time_down_ms: self.time_down as u32,
                normalised_click_point: (
                    self.normalised_click_point.x as f32,
                    self.normalised_click_point.y as f32,
                ),
                dragged_distance_px: self.dragged_distance_px as u32,
                dragged_distance_mm: self.dragged_distance_mm as u32,
            }
        } else if r#type.is_scroll_event() {
            Event::Scroll {
                base,
                scroll_delta: (self.scroll_delta_x as i32, self.scroll_delta_y as i32),
            }
        } else {
            return Err("Unhandled event type".into());
        };

        Ok(event)
    }
}
