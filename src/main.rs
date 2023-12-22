mod defs;
use defs::{ActionEvent, ActionEventC};
use once_cell::sync::Lazy;
use parking_lot::Mutex;
use rusqlite::Connection;
mod utils;
use utils::store_action_event;

static DB_PATH: &str = "action_events.db";
static DB: Lazy<Mutex<Connection>> = Lazy::new(|| Mutex::new(Connection::open(DB_PATH).unwrap()));

#[no_mangle]
pub unsafe extern "C" fn action_event_delegate(c_struct: *mut ActionEventC) {
    let ae: ActionEvent = (*c_struct).tidy_up();

    match store_action_event(&DB, &ae) {
        Ok(_) => println!("Stored action event."),
        Err(e) => println!("Error storing action event: {}", e),
    }
    println!("{:#?}", ae);

    if ae.key_code == 0 {
        let count: i32 = DB
            .lock()
            .query_row("SELECT count(*) FROM staged_inputs", [], |row| row.get(0))
            .unwrap();
        println!("count: {}", count);

        // For each process, get the number of events
        let mut process_counts: Vec<(String, i32)> = DB
            .lock()
            .prepare(
                "SELECT processes.name, count(*) FROM staged_inputs
                 INNER JOIN processes ON processes.id = staged_inputs.process_id
                 GROUP BY process_id",
            )
            .unwrap()
            .query_map([], |row| Ok((row.get(0)?, row.get(1)?)))
            .unwrap()
            .map(|r| r.unwrap())
            .collect();

        process_counts.sort_by(|a, b| b.1.cmp(&a.1));

        for (process_name, count) in process_counts {
            println!("{}: {}", process_name, count);
        }
    }
}

extern "C" {
    fn registerTap();
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    DB.lock().execute(
        "CREATE TABLE IF NOT EXISTS processes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE
         )",
        [],
    )?;

    DB.lock().execute(
        "CREATE TABLE IF NOT EXISTS staged_inputs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                time_down_ms INTEGER,
                type INTEGER,
                key_code INTEGER,
                mouse_x REAL,
                mouse_y REAL,
                dragged_distance_px INTEGER,
                is_builtin_display INTEGER,
                is_main_display INTEGER,
                process_id INTEGER,
                execution_time_us INTEGER,
                FOREIGN KEY(process_id) REFERENCES processes(id)
             )",
        [],
    )?;

    unsafe { registerTap() };
    println!("Rust is exiting.");

    Ok(())
}
