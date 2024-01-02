#![feature(slice_split_once)]
#![feature(str_split_remainder)]

mod defs;
mod migrations;
mod utils;
use defs::event::Event;

use parking_lot::{Mutex, RwLock};
use utils::{init_databases, store_action_event};

pub static DB_AGGREGATE: Mutex<Option<rusqlite::Connection>> = Mutex::new(None);

static MAX_STAGED_KEYPRESS_COUNT: isize = 10; // Dev
static STAGED_KEYPRESS_COUNT: RwLock<isize> = RwLock::new(0);
static mut CURRENT_WORD: Vec<Event> = Vec::new();

pub fn handle_event(event: Event) {
    match event {
        Event::Keyboard {
            base,
            time_down_ms,
            key_code,
            key_char,
        } => println!("{time_down_ms}"),
        _ => {}
    }

    // if event.base.r#type.is_keyboard_event() {
    //     *STAGED_KEYPRESS_COUNT.write() += 1;

    //     if *STAGED_KEYPRESS_COUNT.read() >= MAX_STAGED_KEYPRESS_COUNT {
    //         let hour = chrono::Local::now().hour();

    //         *STAGED_KEYPRESS_COUNT.write() = 0;
    //     }
    // }

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
    fn registerTap();
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    init_databases()?;
    unsafe { registerTap() };

    println!("Rust is exiting.");
    Ok(())
}
