use super::migrations::run_pending_migrations;
use crate::defs::event::Event;
use rusqlite::Connection;

pub fn init_databases() -> Result<(), Box<dyn std::error::Error>> {
    let aggregate_db_url = if cfg!(debug_assertions) {
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

pub fn _store_action_event(_event: &Event) -> Result<(), Box<dyn std::error::Error>> {
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

    Ok(())
}

#[no_mangle]
pub unsafe extern "C" fn action_event_delegate(c_struct: *mut crate::defs::c::ActionEventC) {
    let ae = match (*c_struct).tidy_up() {
        Ok(ae) => ae,
        Err(e) => {
            println!("Error tidying up action event: {e}");
            return;
        }
    };

    crate::handle_event(ae);
}
