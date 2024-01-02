use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct Migration {
    pub timestamp: u64,
    pub name: String,
    pub up: String,
    pub down: String,
}

pub static MIGRATIONS_PATH: &str = "migrations";
pub static COMPILED_MIGRATION_FILENAME: &str = "migrations.csv";
