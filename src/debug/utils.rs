// use crate::{defs::event::EventDetail, Event};

// pub fn debug_event_callback(event: Event) {
//     match event.detail {
//         EventDetail::Keyboard {
//             key_char,
//             time_down_ms,
//             ..
//         } => unsafe {
//             super::KEY_CHAR.replace(key_char);
//             super::KEY_TIME_DOWN_MS.replace(time_down_ms);
//         },
//         EventDetail::MouseMove {
//             mouse_angle,
//             mouse_speed_kph,
//             ..
//         } => {
//             if let Some(mouse_angle_rad) = mouse_angle {
//                 unsafe {
//                     super::MOUSE_DIR_EMOJI.replace(dir_emoji_from_rad(mouse_angle_rad));
//                     super::MOUSE_SPEED.replace((mouse_speed_kph * 10_f32).round() / 10_f32);
//                 }
//             }
//         }
//         EventDetail::Scroll {
//             scroll_angle,
//             scroll_speed_kph,
//             ..
//         } => {
//             if let Some(scroll_angle_rad) = scroll_angle {
//                 unsafe {
//                     super::SCROLL_DIR_EMOJI.replace(dir_emoji_from_rad(scroll_angle_rad));
//                     super::SCROLL_SPEED.replace((scroll_speed_kph * 10_f32).round() / 10_f32);
//                 }
//             }
//         }
//         _ => {}
//     }
// }

pub fn dir_emoji_from_rad(rad: f32) -> &'static str {
    static EMOJI_DIRS: [&str; 8] = ["⬆️", "↗️", "➡️", "↘️", "⬇️", "↙️", "⬅️", "↖️"];

    let mut angle_deg = rad.to_degrees() + 90_f32;
    angle_deg += 360_f32;
    angle_deg %= 360_f32;
    let dir = EMOJI_DIRS[(angle_deg / 45_f32) as usize];
    dir
}
