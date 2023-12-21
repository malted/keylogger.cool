#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
struct CGEventTypeC(u32); //TODO: enum

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
struct CGPointC {
    x: f64,
    y: f64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
struct TimespecC {
    tv_sec: i64,
    tv_nsec: i64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ActionEventC {
    time_down: i64, // ms
    r#type: CGEventTypeC,
    key_code: i16,
    normalised_click_point: CGPointC,
    is_builtin_display: bool,
    is_main_display: bool,
    function_start: TimespecC,
}
impl ActionEventC {
    pub fn tidy_up(&self) -> ActionEvent {
        use std::time::{SystemTime, UNIX_EPOCH};

        // Get difference between now and function start
        let now = SystemTime::now().duration_since(UNIX_EPOCH).unwrap();
        let diff_sec_ex = now.as_secs() as i64 - self.function_start.tv_sec;
        let diff_nsec_ex = now.subsec_nanos() as i64 - self.function_start.tv_nsec;

        let diff_us = diff_sec_ex * 1_000_000 + diff_nsec_ex / 1_000;

        ActionEvent {
            time_down_ms: self.time_down as i32,
            r#type: self.r#type.0,
            key_code: self.key_code,
            normalised_click_point: (
                (self.normalised_click_point.x) as i32,
                (self.normalised_click_point.y) as i32,
            ),
            is_builtin_display: self.is_builtin_display,
            is_main_display: self.is_main_display,
            execution_time_us: diff_us as i32,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ActionEvent {
    pub time_down_ms: i32,
    pub r#type: u32,
    pub key_code: i16,
    pub normalised_click_point: (i32, i32),
    pub is_builtin_display: bool,
    pub is_main_display: bool,
    pub execution_time_us: i32,
}
