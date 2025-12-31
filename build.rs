extern crate bindgen;
extern crate cc;
extern crate pkg_config;

use std::{path::PathBuf};
use dotenvy::dotenv;

fn main()
{
  println!("cargo::rerun-if-changed=src/av/scan_media_streams.c");
  println!("cargo::rerun-if-changed=src/av/scan_media_streams.h");
  println!("cargo::rerun-if-changed=src/av/process_media.c");
  println!("cargo::rerun-if-changed=src/av/process_media.h");
  println!("cargo::rerun-if-changed=src/av/proc.c");
  println!("cargo::rerun-if-changed=src/av/proc.h");
  println!("cargo::rerun-if-changed=src/av/input.c");
  println!("cargo::rerun-if-changed=src/av/input.h");
  println!("cargo::rerun-if-changed=src/av/output.c");
  println!("cargo::rerun-if-changed=src/av/output.h");
  println!("cargo::rerun-if-changed=src/av/hdr.c");
  println!("cargo::rerun-if-changed=src/av/hdr.h");
  println!("cargo::rerun-if-changed=src/av/swr.c");
  println!("cargo::rerun-if-changed=src/av/swr.h");
  println!("cargo::rerun-if-changed=src/av/fsc.c");
  println!("cargo::rerun-if-changed=src/av/fsc.h");
  println!("cargo::rerun-if-changed=src/av/deint.c");
  println!("cargo::rerun-if-changed=src/av/deint.h");
  println!("cargo::rerun-if-changed=src/av/utils.c");
  println!("cargo::rerun-if-changed=src/av/utils.h");

  dotenv().ok();

  let avformat = pkg_config::Config::new().probe("libavformat")
    .expect("Failed to find libavformat");

  let avcodec = pkg_config::Config::new().probe("libavcodec")
    .expect("Failed to find libavcodec");

  let swresample = pkg_config::Config::new().probe("libswresample")
    .expect("Failed to find libswresample");

  let avfilter = pkg_config::Config::new().probe("libavfilter")
    .expect("Failed to find libavfilter");

  let avutil = pkg_config::Config::new().probe("libavutil")
    .expect("Failed to find libavutil");

  let sqlite3 = pkg_config::Config::new().probe("sqlite3")
    .expect("Failed to find sqlite3");

  let uuid = pkg_config::Config::new().probe("uuid")
    .expect("Failed to find uuid");

  let database_url =
    std::env::var("DATABASE_URL").expect("DATABASE_URL must be set");

  let mut cc_builder = cc::Build::new();

  cc_builder.file("src/av/scan_media_streams.c");
  cc_builder.file("src/av/process_media.c");
  cc_builder.file("src/av/proc.c");
  cc_builder.file("src/av/input.c");
  cc_builder.file("src/av/output.c");
  cc_builder.file("src/av/hdr.c");
  cc_builder.file("src/av/swr.c");
  cc_builder.file("src/av/fsc.c");
  cc_builder.file("src/av/deint.c");
  cc_builder.file("src/av/utils.c");
  cc_builder.define("DATABASE_URL", format!("\"{}\"", database_url).as_str());

  for path in &avformat.include_paths {
    cc_builder.include(path);
  }

  for path in &avcodec.include_paths {
    cc_builder.include(path);
  }

  for path in &swresample.include_paths {
    cc_builder.include(path);
  }

  for path in &avfilter.include_paths {
    cc_builder.include(path);
  }

  for path in &avutil.include_paths {
    cc_builder.include(path);
  }

  for path in &sqlite3.include_paths {
    cc_builder.include(path);
  }

  for path in &uuid.include_paths {
    cc_builder.include(path);
  }

  cc_builder.compile("av.a");

  for path in &avformat.link_paths {
    println!("cargo:rustc-link-search={}", path.to_string_lossy());
  }

  for library in &avformat.libs {
    println!("cargo:rustc-link-lib={}", library);
  }

  for path in &avcodec.link_paths {
    println!("cargo:rustc-link-search={}", path.to_string_lossy());
  }

  for library in &avcodec.libs {
    println!("cargo:rustc-link-lib={}", library);
  }

  for path in &swresample.link_paths {
    println!("cargo:rustc-link-search={}", path.to_string_lossy());
  }

  for library in &swresample.libs {
    println!("cargo:rustc-link-lib={}", library);
  }

  for path in &avfilter.link_paths {
    println!("cargo:rustc-link-search={}", path.to_string_lossy());
  }

  for library in &avfilter.libs {
    println!("cargo:rustc-link-lib={}", library);
  }

  for path in &avutil.link_paths {
    println!("cargo:rustc-link-search={}", path.to_string_lossy());
  }

  for library in &avutil.libs {
    println!("cargo:rustc-link-lib={}", library);
  }

  for path in &sqlite3.link_paths {
    println!("cargo:rustc-link-search={}", path.to_string_lossy());
  }

  for library in &sqlite3.libs {
    println!("cargo:rustc-link-lib={}", library);
  }

  for path in &uuid.link_paths {
    println!("cargo:rustc-link-search={}", path.to_string_lossy());
  }

  for library in &uuid.libs {
    println!("cargo:rustc-link-lib={}", library);
  }

  let mut builder = bindgen::Builder::default()
    .header("src/av/av.h")
    .blocklist_item("FP_NAN")
    .blocklist_item("FP_INFINITE")
    .blocklist_item("FP_ZERO")
    .blocklist_item("FP_SUBNORMAL")
    .blocklist_item("FP_NORMAL")
    .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()));

  for path in &avformat.include_paths {
    println!("path: {}", path.to_string_lossy());
    builder = builder.clang_arg("-I").clang_arg(path.to_string_lossy());
  }

  for path in &avcodec.include_paths {
    println!("path: {}", path.to_string_lossy());
    builder = builder.clang_arg("-I").clang_arg(path.to_string_lossy());
  }

  for path in &swresample.include_paths {
    println!("path: {}", path.to_string_lossy());
    builder = builder.clang_arg("-I").clang_arg(path.to_string_lossy());
  }

  for path in &avfilter.include_paths {
    println!("path: {}", path.to_string_lossy());
    builder = builder.clang_arg("-I").clang_arg(path.to_string_lossy());
  }

  for path in &avutil.include_paths {
    println!("path: {}", path.to_string_lossy());
    builder = builder.clang_arg("-I").clang_arg(path.to_string_lossy());
  }

  for path in &sqlite3.include_paths {
    println!("path: {}", path.to_string_lossy());
    builder = builder.clang_arg("-I").clang_arg(path.to_string_lossy());
  }

  for path in &uuid.include_paths {
    println!("path: {}", path.to_string_lossy());
    builder = builder.clang_arg("-I").clang_arg(path.to_string_lossy());
  }

  let bindings = builder.generate().expect("Failed to generate bindings.");

  let out_path = PathBuf::from("src");

  bindings.write_to_file(out_path.join("av.rs"))
    .expect("Failed to write bindings.");
}
