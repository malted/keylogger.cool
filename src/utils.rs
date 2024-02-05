use super::migrations::run_pending_migrations;
use crate::{
    defs::c::{MouseMoveEventC, TimespecC},
    defs::event::MouseMoveEvent,
};
use rusqlite::Connection;
use status_bar::{ns_alert, sync_infinite_event_loop, Menu, MenuItem, StatusItem};
use std::sync::{
    atomic::{AtomicU8, Ordering::SeqCst},
    Arc,
};

pub fn init_databases() -> Result<(), Box<dyn std::error::Error>> {
    let aggregate_db_url = if cfg!(test) {
        ":memory:"
    } else if cfg!(debug_assertions) {
        "./aggregate-debug.sqlite3"
    } else {
        "~/.klcool/aggregate.sqlite3"
    };
    let aggregate_connection = Connection::open(aggregate_db_url)
        .expect("failed to open connection to aggregate database");

    crate::DB_AGGREGATE.lock().replace(aggregate_connection);

    run_pending_migrations()?;

    Ok(())
}

// pub fn _store_action_event(_event: &Event) -> Result<(), Box<dyn std::error::Error>> {
// let conn = db.lock();
// let process_id: Option<i32> = conn
//     .query_row(
//         "SELECT id FROM processes WHERE name = ?1",
//         [event.process_name.as_str()],
//         |row| row.get(0),
//     )
//     .ok();

// let process_id = match process_id {
//     Some(id) => id,
//     None => {
//         // Insert new process_name and get its id
//         conn.execute(
//             "INSERT INTO processes (name) VALUES (?1)",
//             [&event.process_name],
//         )?;
//         conn.last_insert_rowid() as i32
//     }
// };

// conn
//     .execute(
//             "INSERT INTO staged_inputs (time_down_ms, type, key_code, mouse_x, mouse_y, dragged_distance_px, is_builtin_display, is_main_display, process_id, execution_time_us)
//              VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
//              (
//                 event.time_down_ms,
//                 event.r#type,
//                 event.key_code,
//                 event.normalised_click_point.0,
//                 event.normalised_click_point.1,
//                 event.dragged_distance_px,
//                 event.is_builtin_display as i32,
//                 event.is_main_display as i32,
//                 process_id,
//                 event.execution_time_us
//              ),
//         )?;

//     Ok(())
// }

static mut PREVIOUS_EXECUTION_RANGE: Option<(u128, u128)> = None;

// #[no_mangle]
// pub unsafe extern "C" fn action_event_delegate(c_struct: *mut crate::defs::c::ActionEventC) {
//     let ae = match (*c_struct).tidy_up() {
//         Ok(ae) => ae,
//         Err(e) => {
//             log::error!("Error tidying up action event: {e}");
//             return;
//         }
//     };

//     let now = std::time::SystemTime::now()
//         .duration_since(std::time::UNIX_EPOCH)
//         .expect("time went backwards");

//     if let Some(range) = PREVIOUS_EXECUTION_RANGE {
//         let diff_us = (range.1 - range.0) as u32;

//         if cfg!(debug_assertions) {
//             debug::LAST_EXECUTION_TIME_US.replace(diff_us);
//         }
//     }

//     crate::handle_event(ae.clone());

//     PREVIOUS_EXECUTION_RANGE = Some((ae.base.start_time_us, now.as_micros()));
// }

#[no_mangle]
pub unsafe extern "C" fn pass_mouse_move_events(
    c_events: *const [MouseMoveEventC; 255],
    event_count: std::ffi::c_int,
    function_start: TimespecC,
) {
    let mut angle = 0;
    let mut event: MouseMoveEvent = c_events
        .as_ref()
        .unwrap()
        .iter()
        .take(event_count as usize)
        .map(|e| e.tidy_up())
        .reduce(|acc, e| {
            angle += e.angle as u128 * e.distance_px as u128;
            MouseMoveEvent {
                base: acc.base,
                distance_px: acc.distance_px + e.distance_px,
                distance_mm: acc.distance_mm + e.distance_mm,
                angle: 0,
                velocity_kph: acc.velocity_kph.max(e.velocity_kph),
            }
        })
        .expect("the iterator to have at least one element");
    event.angle = (angle / event.distance_px as u128) as u16;

    // println!("\t\t\t TWO {:?}", function_start.from_now_micros());

    // let distance_mm_sum = events.clone().map(|e| e.distance_mm).sum();

    // let angle_weighted_mean: u32 = events
    //     .clone()
    //     .map(|e| e.angle as u32 * e.distance_px)
    //     .sum::<u32>()
    //     / distance_px_sum;

    // let max_velocity = events
    //     .clone()
    //     .map(|e| e.velocity_kph)
    //     .reduce(|acc, e| acc.max(e))
    //     .expect("a max velocity");

    // let mut base = events.clone().nth(0).unwrap().base;
    // base.eventc_aggregate_count = Some(event_count as u8);

    // let aggregate = MouseMoveEvent {
    //     base,
    //     distance_px: distance_px_sum,
    //     distance_mm: distance_mm_sum,
    //     angle: angle_weighted_mean as i16,
    //     velocity_kph: max_velocity,
    // };

    // println!("\t\t\t FIN {:?}", function_start.from_now_micros());
    // println!("{:#?}", event);
}

#[no_mangle]
pub unsafe extern "C" fn log_error(msg: *const std::os::raw::c_char) {
    log::error!("{}", std::ffi::CStr::from_ptr(msg).to_str().unwrap());
}
#[no_mangle]
pub unsafe extern "C" fn log_warn(msg: *const std::os::raw::c_char) {
    log::warn!("{}", std::ffi::CStr::from_ptr(msg).to_str().unwrap());
}
#[no_mangle]
pub unsafe extern "C" fn log_info(msg: *const std::os::raw::c_char) {
    log::info!("{}", std::ffi::CStr::from_ptr(msg).to_str().unwrap());
}
#[no_mangle]
pub unsafe extern "C" fn log_debug(msg: *const std::os::raw::c_char) {
    log::debug!("{}", std::ffi::CStr::from_ptr(msg).to_str().unwrap());
}
#[no_mangle]
pub unsafe extern "C" fn log_trace(msg: *const std::os::raw::c_char) {
    log::trace!("{}", std::ffi::CStr::from_ptr(msg).to_str().unwrap());
}

pub fn init_statusbar() {
    let (sender, receiver) = std::sync::mpsc::channel::<()>();

    let unread_indicator = Arc::new(std::sync::atomic::AtomicBool::new(false));
    let idx = Arc::new(AtomicU8::new(0)); // fetch_add wraps! Don't fret.
    let idx2 = idx.clone();

    // thread that sends command to event loop
    std::thread::spawn(move || loop {
        idx.fetch_add(1, SeqCst);
        sender.send(()).unwrap();

        std::thread::sleep(std::time::Duration::from_secs(1));
    });

    let status_item = std::sync::Arc::new(std::cell::RefCell::new(StatusItem::new(
        "",
        Menu::new(vec![]),
    )));

    let running_text = format!(
        "Keylogger.cool v{} is running  🎉",
        env!("CARGO_PKG_VERSION")
    );

    let si = status_item.clone();
    sync_infinite_event_loop(receiver, move |_| {
        if unread_indicator.load(SeqCst) {
            si.borrow_mut().set_image("keyboard.badge.ellipsis.fill");
            si.borrow_mut().set_menu(Menu::new(vec![
                MenuItem::new(&running_text, None, None),
                MenuItem::new(
                    format!("Update available (v{})", "1.0.1"),
                    None,
                    Some(Menu::new(vec![
                        MenuItem::new(
                            "View release on GitHub",
                            Some(Box::new(|| {
                                webbrowser::open(
                                    "https://github.com/malted/keylogger.cool/releases",
                                )
                                .unwrap()
                            })),
                            None,
                        ),
                        MenuItem::new(
                            "Download & install update",
                            Some(Box::new(|| {
                                ns_alert("Not implemented yet", "Automatic updates coming soon!")
                            })),
                            None,
                        ),
                    ])),
                ),
            ]));
        } else {
            match idx2.load(SeqCst) % 2 == 0 {
                true => si.borrow_mut().set_image("keyboard.badge.eye"),
                false => si.borrow_mut().set_image("keyboard"),
            }
            si.borrow_mut()
                .set_menu(Menu::new(vec![MenuItem::new(&running_text, None, None)]));
        }
    });
}
