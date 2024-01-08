use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct Migration {
    pub timestamp: u64,
    pub name: String,
    pub up: String,
    pub down: String,
}
#[allow(dead_code)]
pub static MIGRATIONS_PATH: &str = "migrations";
#[allow(dead_code)]
pub static COMPILED_MIGRATION_FILENAME: &str = "migrations.csv";
pub static DELIMITER: char = '~';
