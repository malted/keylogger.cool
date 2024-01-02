#[path = "src/defs/migrations.rs"]
mod migrations;
use migrations::{Migration, COMPILED_MIGRATION_FILENAME, DELIMITER, MIGRATIONS_PATH};

use std::{env, fs, path::Path};

fn main() {
    encode_migrations().expect("failed to encode the migrations");

    cc::Build::new()
        .file("src/tap.c")
        .file("src/DisplayConfiguration.m")
        .compile("tap");

    println!("cargo:rerun-if-changed=src/*");

    println!("cargo:rustc-link-lib=framework=ApplicationServices");
    println!("cargo:rustc-link-lib=framework=AppKit");
    println!("cargo:rustc-link-lib=framework=Carbon");
}

pub fn encode_migrations() -> Result<(), Box<dyn std::error::Error>> {
    let mut migrations: Vec<Migration> = Vec::new();

    for migration in fs::read_dir(MIGRATIONS_PATH)? {
        let path = migration?.path();

        let (timestamp, name) = path
            .file_name()
            .expect("failed to get the migration directory name")
            .to_str()
            .expect("failed to convert the migration directory name to a &str slice")
            .split_once("-")
            .expect("failed to split the migration directory name by \"-\" once");
        let timestamp = timestamp.parse::<u64>().expect("could not parse the migration directory timestamp as a u64 (it should be UNIX seconds).");

        let up = fs::read_to_string(path.join("up.sql"))?;
        let down = fs::read_to_string(path.join("down.sql"))?;

        if up.trim().is_empty() || down.trim().is_empty() {
            panic!("up.sql and down.sql must not be empty");
        }

        migrations.push(Migration {
            timestamp,
            name: name.to_string(),
            up,
            down,
        });
    }

    // Write the migrations to a csv file
    let out_path = Path::new(&env::var("OUT_DIR")?).join(COMPILED_MIGRATION_FILENAME);
    let mut wtr = csv::WriterBuilder::new()
        .delimiter(DELIMITER as u8)
        .from_path(&out_path)?;

    for migration in migrations {
        wtr.serialize(migration)?;
    }
    wtr.flush()?;

    Ok(())
}
