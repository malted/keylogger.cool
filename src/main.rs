use once_cell::sync::Lazy;
use parking_lot::Mutex;
mod defs;
use defs::*;

static DB_PATH: &str = ":memory:";
static DB: Lazy<Mutex<sqlite::Connection>> =
    Lazy::new(|| Mutex::new(sqlite::open(DB_PATH).unwrap()));

#[no_mangle]
pub unsafe extern "C" fn action_event_delegate(c_struct: *mut ActionEventC) {
    let ae: ActionEvent = (*c_struct).tidy_up();
    println!("{:?}\n", ae);

    //TODO: Add to database
}

extern "C" {
    fn registerTap();
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    DB.lock().execute(
        "
            CREATE TABLE IF NOT EXISTS staged_inputs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                time_down INTEGER,
                key_code INTEGER,
                mouse_x REAL,
                mouse_y REAL
            )
        ",
    )?;

    unsafe { registerTap() };
    println!("Rust is exiting.");

    Ok(())
}
