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
    mouse_distance_px: c_double,
    mouse_distance_mm: c_double,
    mouse_angle: c_double,
    mouse_speed_kph: c_double,
    scroll_delta_x: c_int,
    scroll_delta_y: c_int,
    scroll_angle: c_double,
    scroll_speed_kph: c_double,
    dragged_distance_px: c_double,
    dragged_distance_mm: c_double,
    drag_angle: c_double,
    drag_speed_kph: c_double,
    is_builtin_display: bool,
    is_main_display: bool,
    function_start: TimespecC,
    keyboard_layout: *const c_char,
    process_name: *const c_char,
}
impl ActionEventC {
    pub fn tidy_up(&self) -> Result<Event, Box<dyn std::error::Error>> {
        // println!("{:#?}", self);
        let r#type = EventType::from_u32(self.r#type.0);

        let process_name_cstr = unsafe { CStr::from_ptr(self.process_name) };
        let process_name = String::from_utf8_lossy(process_name_cstr.to_bytes()).to_string();

        let keyboard_layout_cstr = unsafe { CStr::from_ptr(self.keyboard_layout) };
        let keyboard_layout = String::from_utf8_lossy(keyboard_layout_cstr.to_bytes()).to_string();

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

        let safe_f32 = |f: f32| {
            if f.is_nan() || !f.is_finite() {
                0_f32
            } else {
                f
            }
        };

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
                keyboard_layout,
            }
        } else if r#type.is_mouse_click_event() {
            Event::MouseClick {
                base,
                time_down_ms: self.time_down as u32,
                normalised_click_point: (
                    safe_f32(self.normalised_click_point.x as f32),
                    safe_f32(self.normalised_click_point.y as f32),
                ),
                dragged_distance_px: self.dragged_distance_px as u32,
                dragged_distance_mm: self.dragged_distance_mm as u32,
                drag_angle: if self.dragged_distance_px == 0_f64 {
                    None
                } else {
                    Some(safe_f32(self.drag_angle as f32))
                },
                drag_speed_kph: safe_f32(self.drag_speed_kph as f32),
            }
        } else if r#type.is_mouse_move_event() {
            Event::MouseMove {
                base,
                distance_px: self.mouse_distance_mm as u32,
                distance_mm: self.mouse_distance_mm as u32,
                mouse_angle: if self.mouse_distance_px == 0_f64 {
                    None
                } else {
                    Some(safe_f32(self.mouse_angle as f32))
                },
                mouse_speed_kph: safe_f32(self.mouse_speed_kph as f32),
            }
        } else if r#type.is_scroll_event() {
            Event::Scroll {
                base,
                scroll_delta: (self.scroll_delta_x as i32, self.scroll_delta_y as i32),
                scroll_angle: if self.scroll_delta_x == 0 && self.scroll_delta_y == 0 {
                    None
                } else {
                    Some(self.scroll_angle as f32)
                },
                scroll_speed_kph: safe_f32(self.scroll_speed_kph as f32),
            }
        } else {
            return Err("Unhandled event type".into());
        };

        Ok(event)
    }
}
