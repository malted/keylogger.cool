use crate::defs::migrations::{Migration, DELIMITER};
// use std::time::{SystemTime, UNIX_EPOCH};

const MIGRATIONS: &'static str = &include_str!(concat!(env!("OUT_DIR"), "/migrations.csv"));

pub fn run_pending_migrations() -> Result<(), Box<dyn std::error::Error>> {
    let mut rdr = csv::ReaderBuilder::new()
        .delimiter(DELIMITER as u8)
        .from_reader(MIGRATIONS.as_bytes());

    let mut migrations: Vec<Migration> = rdr
        .deserialize()
        .collect::<Result<Vec<Migration>, _>>()
        .expect("failed to deserialize the migrations")
        .into_iter()
        .filter(|m| {
            m.timestamp
                > last_executed_migration_timestamp()
                    .expect("failed to get the last executed migration timestamp")
        })
        .collect::<Vec<Migration>>();
    migrations.sort_by(|a, b| a.timestamp.cmp(&b.timestamp));

    for migration in migrations {
        println!("Running migration: {}", migration.name);

        let conn = crate::DB_AGGREGATE.lock();
        let conn = conn.as_ref().expect("don't call this before db init!");

        conn.execute_batch(&migration.up)?;

        // Log it in the migrations table
        conn.execute(
            "INSERT INTO migration (direction, migration_version_timestamp) VALUES ('U', ?)",
            [migration.timestamp as i64],
        )?;
    }

    Ok(())
}

pub fn last_executed_migration_timestamp() -> rusqlite::Result<u64> {
    let conn = crate::DB_AGGREGATE.lock();
    let conn = conn.as_ref().expect("don't call this before db init!");

    let migration_table_count: usize = conn.query_row(
        "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='migration'",
        [],
        |row| row.get(0),
    )?;

    if migration_table_count == 0 {
        return Ok(0);
    } else if migration_table_count > 1 {
        panic!(
            "there should only be one 'migration' table, but there are {}",
            migration_table_count
        );
    }

    conn.query_row(
        "SELECT migration_version_timestamp FROM migration ORDER BY id DESC LIMIT 1",
        [],
        |row| row.get(0),
    )
}
