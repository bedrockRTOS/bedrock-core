// SPDX-License-Identifier: Unlicense
//
// Read Kconfig + .config and emit include/generated/autoconf.h.

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
    let kconfig = PathBuf::from(flag(&args, "--kconfig", "Kconfig"));
    let config  = PathBuf::from(flag(&args, "--config",  ".config"));
    let header  = PathBuf::from(flag(&args, "--header",  "include/generated/autoconf.h"));

    let mut db = kconfig_tools::parse(&kconfig).unwrap_or_else(|e| {
        eprintln!("error: cannot parse {}: {e}", kconfig.display());
        std::process::exit(1);
    });
    kconfig_tools::load_dot_config(&mut db, &config);
    kconfig_tools::write_autoconf(&db, &header).unwrap_or_else(|e| {
        eprintln!("error: cannot write {}: {e}", header.display());
        std::process::exit(1);
    });
    println!("  GEN   {}", header.display());
}
