#[derive(Debug, Clone, Copy, PartialEq)]
pub enum EventType {
    // The null event.
    Null = 0,

    // Mouse events.
    LeftMouseDown = 1,
    LeftMouseUp = 2,
    RightMouseDown = 3,
    RightMouseUp = 4,
    MouseMoved = 5,
    LeftMouseDragged = 6,
    RightMouseDragged = 7,

    // Keyboard events.
    KeyDown = 10,
    KeyUp = 11,
    FlagsChanged = 12,

    // Specialized control devices.
    ScrollWheel = 22,
    TabletPointer = 23,
    TabletProximity = 24,
    OtherMouseDown = 25,
    OtherMouseUp = 26,
    OtherMouseDragged = 27,

    /* Out of band event types.
     * These are delivered to the event tap callback
     * but do not come from any event stream. */
    TapDisabledByTimeout = 0xFFFFFFFE,
    TapDisabledByUserInput = 0xFFFFFFFF,
}
impl EventType {
    pub fn from_u32(value: u32) -> Self {
        unsafe { std::mem::transmute(value) } // Come at me.
    }
    pub fn is_keyboard_event(&self) -> bool {
        matches!(*self, Self::KeyDown | Self::KeyUp)
    }
    pub fn is_mouse_event(&self) -> bool {
        matches!(
            *self,
            Self::LeftMouseDown
                | Self::LeftMouseUp
                | Self::RightMouseDown
                | Self::RightMouseUp
                | Self::MouseMoved
                | Self::LeftMouseDragged
                | Self::RightMouseDragged
        )
    }
    pub fn is_scroll_event(&self) -> bool {
        matches!(*self, Self::ScrollWheel)
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct BaseEvent {
    pub r#type: EventType,
    pub is_builtin_display: bool,
    pub is_main_display: bool,
    pub process_name: String,
    pub execution_time_us: i32,
}

#[derive(Debug)]
pub enum Event {
    Keyboard {
        base: BaseEvent,
        time_down_ms: u32,
        key_code: u16,
        key_char: String,
    },
    Mouse {
        base: BaseEvent,
        time_down_ms: u32,
        normalised_click_point: (f32, f32),
        dragged_distance_px: u32,
        dragged_distance_mm: u32,
    },
    Scroll {
        base: BaseEvent,
        scroll_delta: (i32, i32),
    },
}
