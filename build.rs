use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    cc::Build::new()
        .file("src/tap.c")
        .file("src/DisplayConfiguration.m")
        .compile("tap");

    println!("cargo:rerun-if-changed=src/DisplayConfiguration.h");
    println!("cargo:rerun-if-changed=src/DisplayConfiguration.m");
    println!("cargo:rerun-if-changed=src/tap.c");
    println!("cargo:rerun-if-changed=src/tap.h");

    println!("cargo:rustc-link-lib=framework=ApplicationServices");
    println!("cargo:rustc-link-lib=framework=Foundation");
    println!("cargo:rusto-link-lib=framework=Cocoa");
    println!("cargo:rustc-link-lib=framework=AppKit");
}
