#![feature(slice_split_once)]
#![feature(str_split_remainder)]
#![feature(inline_const)]

mod debug;
mod defs;
mod migrations;
mod utils;
use std::sync::atomic::{AtomicU8, AtomicUsize, Ordering::SeqCst};

use parking_lot::{Mutex, RwLock};
use status_bar::{ns_alert, sync_infinite_event_loop, Menu, MenuItem, StatusItem};
use std::sync::Arc;
use utils::init_databases;

pub static DB_AGGREGATE: Mutex<Option<rusqlite::Connection>> = Mutex::new(None);

const MAX_STAGED_INPUT_COUNT: usize = 100; // Devel only

static MOUSE_MOVE_COUNTER: AtomicUsize = AtomicUsize::new(0);

static LAST_PROCESS_NAME: RwLock<Option<String>> = RwLock::new(None);

// pub fn handle_event(event: Event) {
//     // if cfg!(debug_assertions) {
//     //     debug::debug_event_callback(event.clone());
//     // }

//     match event.detail {
//         EventDetail::Keyboard {
//             keyboard_layout, ..
//         } => {
//             if let Some(_last_process_name) = LAST_PROCESS_NAME.read().clone() {
//                 // Subsequent events
//             } else {
//                 // First event
//             }
//             // if *STAGED_KEYPRESS_COUNT.read() >= MAX_STAGED_KEYPRESS_COUNT {
//             //     let hour = chrono::Local::now().hour();

//             //     *STAGED_KEYPRESS_COUNT.write() = 0;
//             // }
//             LAST_PROCESS_NAME.write().replace(event.base.process_name);
//         }
//         EventDetail::MouseMove { .. } => {
//             log::trace!("Mouse move");
//             STAGED_MOUSE_MOVES.write()[MOUSE_MOVE_COUNTER.fetch_add(1, SeqCst)] = Some(event);

//             if MOUSE_MOVE_COUNTER.load(SeqCst) >= MAX_STAGED_INPUT_COUNT {
//                 log::debug!("Committing mouse move changes to db!");

//                 let ts = std::time::SystemTime::now()
//                     .duration_since(std::time::UNIX_EPOCH)
//                     .unwrap()
//                     .as_secs();
//                 let ts = ts - (ts % 3600); // Round down to nearest hour.

//                 let mut db = DB_AGGREGATE.lock();
//                 let db = db.as_mut().unwrap();

//                 let tx = db.transaction().expect("failed to init transaction");

//                 let mut stmt = tx
//                     .prepare(
//                         "INSERT INTO mouse_move_event (
// 							type,
// 							is_builtin_display,
// 							is_main_display,
// 							process_id,
// 							execution_time_us,
// 							dragged_distance_px,
// 							dragged_distance_mm,
// 							datetime
// 						) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
//                     )
//                     .expect("failed to prepare statement");

//                 for event in STAGED_MOUSE_MOVES.read().iter().flatten() {
//                     if let EventDetail::MouseMove {
//                         distance_mm,
//                         distance_px,
//                         mouse_angle,
//                         mouse_speed_kph,
//                     } = event.detail
//                     {
//                         let process_id = 0;

//                         stmt.execute([
//                             event.base.r#type as i32,
//                             event.base.is_builtin_display as i32,
//                             event.base.is_main_display as i32,
//                             process_id,
//                             event.base.start_time_us as i32,
//                             distance_mm as i32,
//                             distance_px as i32,
//                             ts as i32,
//                         ])
//                         .expect("failed to execute statement");
//                     } else {
//                         panic!("foobar");
//                     }
//                 }

//                 MOUSE_MOVE_COUNTER.store(0, SeqCst);
//             }
//         }
//         _ => {}
//     }

//     // match store_action_event(&event) {
//     //     Ok(_) => (),
//     //     Err(e) => {
//     //         println!("Error storing action event: {}", e);
//     //         return;
//     //     }
//     // }

//     // if ae.key_code == 0 {
//     //     let count: i32 = DB_STAGING
//     //         .lock()
//     //         .query_row("SELECT count(*) FROM staged_inputs", [], |row| row.get(0))
//     //         .unwrap();
//     //     println!("count: {}", count);

//     //     // For each process, get the number of events
//     //     let mut process_counts: Vec<(String, i32)> = DB_STAGING
//     //         .lock()
//     //         .prepare(
//     //             "SELECT processes.name, count(*) FROM staged_inputs
//     //              INNER JOIN processes ON processes.id = staged_inputs.process_id
//     //              GROUP BY process_id",
//     //         )
//     //         .unwrap()
//     //         .query_map([], |row| Ok((row.get(0)?, row.get(1)?)))
//     //         .unwrap()
//     //         .map(|r| r.unwrap())
//     //         .collect();

//     //     process_counts.sort_by(|a, b| b.1.cmp(&a.1));

//     //     for (process_name, count) in process_counts {
//     //         println!("{}: {}", process_name, count);
//     //     }
//     // }
// }

extern "C" {
    fn registerTap() -> isize;
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    env_logger::init();
    init_databases()?;

    std::thread::spawn(|| unsafe { registerTap() });

    utils::init_statusbar();

    // if std::env::args().any(|arg| arg == "--gui") {
    //     std::thread::spawn(|| debug::start_term().unwrap());
    // }

    log::warn!("Rust is exiting.");
    Ok(())
}

#[cfg(test)]
mod tests {
    fn log_init() {
        let _ = env_logger::builder().is_test(true).try_init();
    }

    #[test]
    fn log_smoke() {
        log_init();

        log::info!("Info log from `log_smoke` test");

        assert_eq!(2, 1 + 1);
    }

    #[test]
    fn db_init() {
        log_init();

        super::utils::init_databases().unwrap();
    }

    #[test]
    fn tap_create() {
        log_init();

        use std::sync::atomic::{AtomicU8, Ordering};
        // 0 = not started, 1 = started, 2 = stopped
        let tap_thread_status = std::sync::Arc::new(AtomicU8::new(0));

        let thread_tap_thread_status = tap_thread_status.clone();
        std::thread::spawn(move || {
            thread_tap_thread_status.store(1, Ordering::Relaxed);
            unsafe { super::registerTap() };
            thread_tap_thread_status.store(2, Ordering::Relaxed);
        });

        // Wait for the thread to start
        while tap_thread_status.load(Ordering::Relaxed) == 0 {
            std::thread::sleep(std::time::Duration::from_millis(10));
        }

        // Wait for the tap registration to fail (if it does).
        std::thread::sleep(std::time::Duration::from_millis(500));

        assert_eq!(tap_thread_status.load(Ordering::Relaxed), 1);
    }
}
