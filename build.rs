extern crate bindgen;
extern crate cc;
extern crate pkg_config;

use std::{path::PathBuf};
use dotenvy::dotenv;

fn main()
{
  let profile = std::env::var("PROFILE").unwrap();
  if profile == "release" {
      println!("cargo:warning=Building C code in RELEASE mode");
  } else {
      println!("cargo:warning=Building C code in DEBUG mode");
  }

  println!("cargo::rerun-if-changed=src/av/scan_media_streams.c");
  println!("cargo::rerun-if-changed=src/av/scan_media_streams.h");
  println!("cargo::rerun-if-changed=src/av/pipeline.c");
  println!("cargo::rerun-if-changed=src/av/pipeline.h");
  println!("cargo::rerun-if-changed=src/av/init/proc.c");
  println!("cargo::rerun-if-changed=src/av/init/proc.h");
  println!("cargo::rerun-if-changed=src/av/init/input.c");
  println!("cargo::rerun-if-changed=src/av/init/input.h");
  println!("cargo::rerun-if-changed=src/av/init/output.c");
  println!("cargo::rerun-if-changed=src/av/init/output.h");
  println!("cargo::rerun-if-changed=src/av/tools/hdr.c");
  println!("cargo::rerun-if-changed=src/av/tools/hdr.h");
  println!("cargo::rerun-if-changed=src/av/tools/swr.c");
  println!("cargo::rerun-if-changed=src/av/tools/swr.h");
  println!("cargo::rerun-if-changed=src/av/tools/fsc.c");
  println!("cargo::rerun-if-changed=src/av/tools/fsc.h");
  println!("cargo::rerun-if-changed=src/av/filters/fmt.c");
  println!("cargo::rerun-if-changed=src/av/filters/fmt.h");
  println!("cargo::rerun-if-changed=src/av/filters/deint.c");
  println!("cargo::rerun-if-changed=src/av/filters/deint.h");
  println!("cargo::rerun-if-changed=src/av/filters/burn_in.c");
  println!("cargo::rerun-if-changed=src/av/filters/burn_in.h");
  println!("cargo::rerun-if-changed=src/av/filters/rendition.c");
  println!("cargo::rerun-if-changed=src/av/filters/rendition.h");
  println!("cargo::rerun-if-changed=src/av/filters/volume.c");
  println!("cargo::rerun-if-changed=src/av/filters/volume.h");
  println!("cargo::rerun-if-changed=src/av/utils/utils.c");
  println!("cargo::rerun-if-changed=src/av/utils/utils.h");
  println!("cargo::rerun-if-changed=src/av/utils/common.h");

  dotenv().ok();

  let avformat = pkg_config::Config::new().probe("libavformat")
    .expect("Failed to find libavformat");

  let avcodec = pkg_config::Config::new().probe("libavcodec")
    .expect("Failed to find libavcodec");

  let swscale = pkg_config::Config::new().probe("libswscale")
    .expect("Failed to find libswresample");

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
  cc_builder.file("src/av/pipeline.c");
  cc_builder.file("src/av/init/proc.c");
  cc_builder.file("src/av/init/input.c");
  cc_builder.file("src/av/init/output.c");
  cc_builder.file("src/av/tools/hdr.c");
  cc_builder.file("src/av/tools/swr.c");
  cc_builder.file("src/av/tools/fsc.c");
  cc_builder.file("src/av/filters/fmt.c");
  cc_builder.file("src/av/filters/deint.c");
  cc_builder.file("src/av/filters/burn_in.c");
  cc_builder.file("src/av/filters/rendition.c");
  cc_builder.file("src/av/filters/volume.c");
  cc_builder.file("src/av/utils/utils.c");
  cc_builder.define("DATABASE_URL", format!("\"{}\"", database_url).as_str());

  for path in &avformat.include_paths {
    cc_builder.include(path);
  }

  for path in &avcodec.include_paths {
    cc_builder.include(path);
  }

  for path in &swscale.include_paths {
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

  for path in &swscale.link_paths {
    println!("cargo:rustc-link-search={}", path.to_string_lossy());
  }

  for library in &swscale.libs {
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
    .header("src/av/utils/av.h")
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

  for path in &swscale.include_paths {
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
