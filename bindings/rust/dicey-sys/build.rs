use std::env;
use std::path::{Path, PathBuf};

fn main() {
    let dicey_path = env::var("DICEY_PATH").map(PathBuf::from).unwrap_or_else(|_| {
        let curdir = env::var("CARGO_MANIFEST_DIR").unwrap();

        Path::new(&curdir).join("libdicey")
    });

    let libdir = dicey_path.join("lib");
    let incdir = dicey_path.join("include");

    println!("cargo:rerun-if-env-changed=DICEY_PATH");

    println!("cargo:rustc-link-search={}", libdir.display());
    println!("cargo:rustc-link-lib=dicey");

    let hpath = incdir.join("dicey").join("dicey.h");
    
    let bindings = bindgen::Builder::default()
        .clang_arg(format!("-I{}", incdir.display()))
        .header(hpath.to_string_lossy())
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

        let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
        bindings
            .write_to_file(out_path.join("bindings.rs"))
            .expect("Couldn't write bindings!");
}