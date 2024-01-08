mod term;
mod utils;
pub use {term::start_term, utils::debug_event_callback};

pub static mut MOUSE_DIR_EMOJI: Option<&str> = None;
pub static mut MOUSE_SPEED: Option<f32> = None;

pub static mut SCROLL_DIR_EMOJI: Option<&str> = None;
pub static mut SCROLL_SPEED: Option<f32> = None;

pub static mut KEY_CHAR: Option<String> = None;
pub static mut KEY_TIME_DOWN_MS: Option<u32> = None;

pub static mut LAST_EXECUTION_TIME_US: Option<u32> = None;
