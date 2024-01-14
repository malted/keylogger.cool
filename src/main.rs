#![feature(slice_split_once)]
#![feature(str_split_remainder)]

mod debug;
mod defs;
mod migrations;
mod utils;
use defs::event::{Event, EventDetail};

use parking_lot::{Mutex, RwLock};
use utils::init_databases;

pub static DB_AGGREGATE: Mutex<Option<rusqlite::Connection>> = Mutex::new(None);

// static MAX_STAGED_KEYPRESS_COUNT: isize = 100; // Dev
// static STAGED_KEYPRESS_COUNT: RwLock<isize> = RwLock::new(0);

static mut STAGED_INPUTS: Vec<Event> = Vec::new();
static LAST_PROCESS_NAME: RwLock<Option<String>> = RwLock::new(None);

// Mouse move events stage
// struct EventMMAgregateStage {}

pub fn handle_event(event: Event) {
    // println!("{:?}", &event);

    unsafe { STAGED_INPUTS.push(event.clone()) };

    #[cfg(debug_assertions)]
    debug::debug_event_callback(event.clone());

    match event.detail {
        EventDetail::Keyboard {
            keyboard_layout, ..
        } => {
            if let Some(_last_process_name) = LAST_PROCESS_NAME.read().clone() {
                // Subsequent events
            } else {
                // First event
            }
            // if *STAGED_KEYPRESS_COUNT.read() >= MAX_STAGED_KEYPRESS_COUNT {
            //     let hour = chrono::Local::now().hour();

            //     *STAGED_KEYPRESS_COUNT.write() = 0;
            // }
            LAST_PROCESS_NAME.write().replace(event.base.process_name);
        }
        EventDetail::MouseMove { .. } => {
            log::trace!("Mouse move");
        }
        EventDetail::Scroll { .. } => {}
        _ => {}
    }

    // match store_action_event(&event) {
    //     Ok(_) => (),
    //     Err(e) => {
    //         println!("Error storing action event: {}", e);
    //         return;
    //     }
    // }

    // if ae.key_code == 0 {
    //     let count: i32 = DB_STAGING
    //         .lock()
    //         .query_row("SELECT count(*) FROM staged_inputs", [], |row| row.get(0))
    //         .unwrap();
    //     println!("count: {}", count);

    //     // For each process, get the number of events
    //     let mut process_counts: Vec<(String, i32)> = DB_STAGING
    //         .lock()
    //         .prepare(
    //             "SELECT processes.name, count(*) FROM staged_inputs
    //              INNER JOIN processes ON processes.id = staged_inputs.process_id
    //              GROUP BY process_id",
    //         )
    //         .unwrap()
    //         .query_map([], |row| Ok((row.get(0)?, row.get(1)?)))
    //         .unwrap()
    //         .map(|r| r.unwrap())
    //         .collect();

    //     process_counts.sort_by(|a, b| b.1.cmp(&a.1));

    //     for (process_name, count) in process_counts {
    //         println!("{}: {}", process_name, count);
    //     }
    // }
}

extern "C" {
    fn registerTap() -> isize;
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    env_logger::init();
    init_databases()?;

    if cfg!(debug_assertions) && std::env::args().any(|arg| arg == "--gui") {
        std::thread::spawn(move || {
            debug::start_term().unwrap();
        });
    }

    unsafe { registerTap() };

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
