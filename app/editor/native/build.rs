extern crate napi_build;

fn main() {
    napi_build::setup();

    // 编译 adapter 最小桩（9 个符号）
    cc::Build::new()
        .file("adapter_stubs.c")
        .compile("adapter_stubs");

    let base = std::env::var("CARGO_MANIFEST_DIR").unwrap_or_default();

    // 主构建目录
    let main = format!("{}/../../../build/lib/Release", base);
    println!("cargo:rustc-link-search={}", main);

    // CI 构建目录
    let ci_dir = format!("{}/../../../build_ci_test", base);
    if std::path::Path::new(&ci_dir).exists() {
        println!("cargo:rustc-link-search={}/lib/Release", ci_dir);
    }

    // 薄库 + 其 CMake 依赖（STATIC lib 的 target_link_libraries 不传播到 DLL 链接）
    println!("cargo:rustc-link-lib=bs_config_declare_ffi");
    println!("cargo:rustc-link-lib=bs_db_mgmt");
    println!("cargo:rustc-link-lib=bs_db_core");
    println!("cargo:rustc-link-lib=bs_kernel_schema");
    println!("cargo:rustc-link-lib=bs_kernel_gate_chain");
}
