// SPDX-License-Identifier: Unlicense
//
// Apply a defconfig file: write .config and regenerate autoconf.h.

use std::path::PathBuf;

fn flag(args: &[String], name: &str, default: &str) -> String {
    let mut it = args.iter().peekable();
    while let Some(a) = it.next() {
        if a == name {
            if let Some(v) = it.next() {
                return v.clone();
            }
        }
    }
    default.to_owned()
}

fn main() {
    let args: Vec<String> = std::env::args().collect();

    let defcfg = args.get(1).cloned().unwrap_or_else(|| {
        eprintln!("usage: defconfig <defconfig-file> [--kconfig F] [--config F] [--header F]");
        std::process::exit(1);
    });

    let kconfig = PathBuf::from(flag(&args, "--kconfig", "Kconfig"));
    let config  = PathBuf::from(flag(&args, "--config",  ".config"));
    let header  = PathBuf::from(flag(&args, "--header",  "include/generated/autoconf.h"));

    let mut db = kconfig_tools::parse(&kconfig).unwrap_or_else(|e| {
        eprintln!("error: cannot parse {}: {e}", kconfig.display());
        std::process::exit(1);
    });
    kconfig_tools::apply_defconfig(&mut db, &PathBuf::from(&defcfg)).unwrap_or_else(|e| {
        eprintln!("error: cannot apply {defcfg}: {e}");
        std::process::exit(1);
    });
    kconfig_tools::save_dot_config(&db, &config).unwrap_or_else(|e| {
        eprintln!("error: cannot write {}: {e}", config.display());
        std::process::exit(1);
    });
    kconfig_tools::write_autoconf(&db, &header).unwrap_or_else(|e| {
        eprintln!("error: cannot write {}: {e}", header.display());
        std::process::exit(1);
    });
    println!("  CFG   {}", config.display());
    println!("  GEN   {}", header.display());
}
