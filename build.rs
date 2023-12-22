fn main() {
    cc::Build::new()
        .file("src/tap.c")
        .file("src/DisplayConfiguration.m")
        .compile("tap");

    println!("cargo:rerun-if-changed=src/*");

    println!("cargo:rustc-link-lib=framework=ApplicationServices");
    println!("cargo:rustc-link-lib=framework=AppKit");
}
