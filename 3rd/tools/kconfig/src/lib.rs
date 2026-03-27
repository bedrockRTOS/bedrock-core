// SPDX-License-Identifier: Unlicense
//
// Kconfig parser, .config reader/writer, and autoconf.h generator.

use std::collections::HashMap;
use std::path::Path;
use std::rc::Rc;

pub type Items = Rc<Vec<Item>>;

#[derive(Debug, Clone, PartialEq)]
pub enum SymType {
    Bool,
    Int,
    Unknown,
}

#[derive(Debug, Clone)]
pub enum Value {
    Bool(bool),
    Int(i64),
}

impl Value {
    pub fn as_bool(&self) -> bool {
        match self {
            Value::Bool(b) => *b,
            Value::Int(i) => *i != 0,
        }
    }

    pub fn as_int(&self) -> i64 {
        match self {
            Value::Bool(b) => *b as i64,
            Value::Int(i) => *i,
        }
    }
}

#[derive(Debug, Clone)]
pub struct Symbol {
    pub name: String,
    pub typ: SymType,
    pub prompt: String,
    pub default: Option<Value>,
    pub range: Option<(i64, i64)>,
    pub help: String,
    pub value: Option<Value>,
}

impl Symbol {
    fn new(name: &str) -> Self {
        Symbol {
            name: name.to_owned(),
            typ: SymType::Unknown,
            prompt: String::new(),
            default: None,
            range: None,
            help: String::new(),
            value: None,
        }
    }

    pub fn effective_bool(&self) -> bool {
        self.value
            .as_ref()
            .or(self.default.as_ref())
            .map(|v| v.as_bool())
            .unwrap_or(false)
    }

    pub fn effective_int(&self) -> i64 {
        let i = self
            .value
            .as_ref()
            .or(self.default.as_ref())
            .map(|v| v.as_int())
            .unwrap_or(0);
        if let Some((lo, hi)) = self.range {
            i.max(lo).min(hi)
        } else {
            i
        }
    }
}

#[derive(Debug, Clone)]
pub struct Choice {
    pub name: String,
    pub prompt: String,
    pub default: Option<String>,
    pub option_names: Vec<String>,
}

impl Choice {
    pub fn selected<'a>(&self, symbols: &'a HashMap<String, Symbol>) -> Option<&'a Symbol> {
        for name in &self.option_names {
            if let Some(sym) = symbols.get(name) {
                if sym.value.as_ref().map(|v| v.as_bool()).unwrap_or(false) {
                    return Some(sym);
                }
            }
        }
        if let Some(ref def) = self.default {
            return symbols.get(def.as_str());
        }
        self.option_names.first().and_then(|n| symbols.get(n.as_str()))
    }

    pub fn select(&self, target: &str, symbols: &mut HashMap<String, Symbol>) {
        for name in &self.option_names {
            if let Some(sym) = symbols.get_mut(name) {
                sym.value = Some(Value::Bool(name == target));
            }
        }
    }
}

#[derive(Debug, Clone)]
pub enum Item {
    Sym(String),
    Menu { title: String, items: Items },
    Choice(String),
}

pub struct KconfigDB {
    pub mainmenu: String,
    pub items: Items,
    pub symbols: HashMap<String, Symbol>,
    pub choices: HashMap<String, Choice>,
}

fn unquote(s: &str) -> String {
    let s = s.trim();
    if s.len() >= 2 && s.starts_with('"') && s.ends_with('"') {
        s[1..s.len() - 1].to_owned()
    } else {
        s.to_owned()
    }
}

fn indent_of(line: &str) -> usize {
    line.len() - line.trim_start().len()
}

fn parse_value(s: &str, typ: &SymType) -> Option<Value> {
    match typ {
        SymType::Bool => Some(Value::Bool(matches!(s, "y" | "yes" | "1" | "true"))),
        SymType::Int => s.parse::<i64>().ok().map(Value::Int),
        SymType::Unknown => None,
    }
}

pub fn parse(path: &Path) -> Result<KconfigDB, std::io::Error> {
    let content = std::fs::read_to_string(path)?;
    let lines: Vec<&str> = content.lines().collect();
    let mut db = KconfigDB {
        mainmenu: String::new(),
        items: Rc::new(Vec::new()),
        symbols: HashMap::new(),
        choices: HashMap::new(),
    };
    let mut pos = 0usize;
    let items = parse_block(&lines, &mut pos, &mut db);
    db.items = Rc::new(items);
    Ok(db)
}

fn parse_block(lines: &[&str], pos: &mut usize, db: &mut KconfigDB) -> Vec<Item> {
    let mut items = Vec::new();
    while *pos < lines.len() {
        let line = lines[*pos];
        let trimmed = line.trim();

        if trimmed.is_empty()
            || trimmed.starts_with('#')
            || trimmed.starts_with("/*")
            || trimmed.starts_with('*')
            || trimmed.starts_with("*/")
        {
            *pos += 1;
            continue;
        }

        if let Some(rest) = trimmed.strip_prefix("mainmenu ") {
            db.mainmenu = unquote(rest);
            *pos += 1;
            continue;
        }

        if let Some(rest) = trimmed.strip_prefix("menu ") {
            let title = unquote(rest);
            *pos += 1;
            let sub = parse_block(lines, pos, db);
            items.push(Item::Menu { title, items: Rc::new(sub) });
            continue;
        }

        if trimmed == "endmenu" {
            *pos += 1;
            return items;
        }

        if trimmed.starts_with("choice") {
            let raw_name = trimmed["choice".len()..].trim().to_owned();
            let name = if raw_name.is_empty() {
                format!("__choice_{}", db.choices.len())
            } else {
                raw_name
            };
            *pos += 1;
            parse_choice(lines, pos, &name, db);
            items.push(Item::Choice(name));
            continue;
        }

        if let Some(rest) = trimmed.strip_prefix("config ") {
            let name = rest.trim().to_owned();
            *pos += 1;
            let sym = parse_symbol_body(lines, pos, &name);
            db.symbols.insert(name.clone(), sym);
            items.push(Item::Sym(name));
            continue;
        }

        *pos += 1;
    }
    items
}

fn parse_symbol_body(lines: &[&str], pos: &mut usize, name: &str) -> Symbol {
    let mut sym = Symbol::new(name);
    while *pos < lines.len() {
        let line = lines[*pos];
        let trimmed = line.trim();

        if trimmed.is_empty() || trimmed.starts_with('#') {
            *pos += 1;
            continue;
        }
        if indent_of(line) == 0 {
            break;
        }

        if let Some(rest) = trimmed.strip_prefix("bool") {
            sym.typ = SymType::Bool;
            let p = rest.trim();
            if !p.is_empty() {
                sym.prompt = unquote(p);
            }
        } else if let Some(rest) = trimmed.strip_prefix("int") {
            sym.typ = SymType::Int;
            let p = rest.trim();
            if !p.is_empty() {
                sym.prompt = unquote(p);
            }
        } else if let Some(rest) = trimmed.strip_prefix("default ") {
            sym.default = parse_value(rest.trim(), &sym.typ);
        } else if let Some(rest) = trimmed.strip_prefix("range ") {
            let mut parts = rest.split_whitespace();
            if let (Some(lo), Some(hi)) = (parts.next(), parts.next()) {
                if let (Ok(lo), Ok(hi)) = (lo.parse::<i64>(), hi.parse::<i64>()) {
                    sym.range = Some((lo, hi));
                }
            }
        } else if trimmed.starts_with("depends on") {
            // skip
        } else if trimmed == "help" {
            let help_indent = indent_of(line);
            *pos += 1;
            let mut base: Option<usize> = None;
            let mut help_lines: Vec<String> = Vec::new();
            while *pos < lines.len() {
                let hline = lines[*pos];
                if hline.trim().is_empty() {
                    help_lines.push(String::new());
                    *pos += 1;
                    continue;
                }
                let h_ind = indent_of(hline);
                if h_ind <= help_indent {
                    break;
                }
                let base_ind = *base.get_or_insert(h_ind);
                let content = if hline.len() > base_ind {
                    hline[base_ind..].trim_end()
                } else {
                    hline.trim()
                };
                help_lines.push(content.to_owned());
                *pos += 1;
            }
            while help_lines.last().map(|s: &String| s.is_empty()).unwrap_or(false) {
                help_lines.pop();
            }
            sym.help = help_lines.join("\n");
            continue;
        }

        *pos += 1;
    }
    sym
}

fn parse_choice(lines: &[&str], pos: &mut usize, name: &str, db: &mut KconfigDB) {
    let mut choice = Choice {
        name: name.to_owned(),
        prompt: String::new(),
        default: None,
        option_names: Vec::new(),
    };

    while *pos < lines.len() {
        let line = lines[*pos];
        let trimmed = line.trim();

        if trimmed.is_empty() || trimmed.starts_with('#') {
            *pos += 1;
            continue;
        }

        if trimmed == "endchoice" {
            if let Some(ref def) = choice.default.clone() {
                if let Some(sym) = db.symbols.get_mut(def.as_str()) {
                    if sym.default.is_none() {
                        sym.default = Some(Value::Bool(true));
                    }
                }
            }
            *pos += 1;
            break;
        }

        if let Some(rest) = trimmed.strip_prefix("prompt ") {
            choice.prompt = unquote(rest);
        } else if let Some(rest) = trimmed.strip_prefix("default ") {
            choice.default = Some(rest.trim().to_owned());
        } else if let Some(rest) = trimmed.strip_prefix("config ") {
            let sym_name = rest.trim().to_owned();
            *pos += 1;
            let sym = parse_symbol_body(lines, pos, &sym_name);
            db.symbols.insert(sym_name.clone(), sym);
            choice.option_names.push(sym_name);
            continue;
        }

        *pos += 1;
    }

    db.choices.insert(name.to_owned(), choice);
}

pub fn load_dot_config(db: &mut KconfigDB, path: &Path) {
    let content = match std::fs::read_to_string(path) {
        Ok(c) => c,
        Err(_) => return,
    };
    for line in content.lines() {
        let t = line.trim();
        if let Some(rest) = t.strip_prefix("# CONFIG_") {
            if let Some(name) = rest.strip_suffix(" is not set") {
                if let Some(sym) = db.symbols.get_mut(name) {
                    sym.value = Some(Value::Bool(false));
                }
            }
        } else if let Some(rest) = t.strip_prefix("CONFIG_") {
            if let Some(eq) = rest.find('=') {
                let name = &rest[..eq];
                let val = rest[eq + 1..].trim();
                if let Some(sym) = db.symbols.get_mut(name) {
                    sym.value = match sym.typ {
                        SymType::Bool => Some(Value::Bool(val == "y" || val == "1")),
                        SymType::Int => val.parse::<i64>().ok().map(Value::Int),
                        SymType::Unknown => None,
                    };
                }
            }
        }
    }
}

pub fn apply_defconfig(db: &mut KconfigDB, path: &Path) -> Result<(), std::io::Error> {
    let content = std::fs::read_to_string(path)?;
    for line in content.lines() {
        let t = line.trim();
        if t.is_empty() || t.starts_with('#') {
            continue;
        }
        if let Some(rest) = t.strip_prefix("CONFIG_") {
            if let Some(eq) = rest.find('=') {
                let name = &rest[..eq];
                let val = rest[eq + 1..].trim();
                if let Some(sym) = db.symbols.get_mut(name) {
                    sym.value = match sym.typ {
                        SymType::Bool => Some(Value::Bool(val == "y" || val == "1")),
                        SymType::Int => val.parse::<i64>().ok().map(Value::Int),
                        SymType::Unknown => None,
                    };
                }
            }
        }
    }
    Ok(())
}

pub fn save_dot_config(db: &KconfigDB, path: &Path) -> Result<(), std::io::Error> {
    let mut out = String::from("#\n# Automatically generated — do not edit.\n#\n\n");
    collect_config(&db.items, &db.symbols, &db.choices, &mut out);
    std::fs::write(path, out)
}

fn collect_config(
    items: &[Item],
    symbols: &HashMap<String, Symbol>,
    choices: &HashMap<String, Choice>,
    out: &mut String,
) {
    for item in items {
        match item {
            Item::Sym(name) => {
                if let Some(sym) = symbols.get(name) {
                    match sym.typ {
                        SymType::Bool => {
                            if sym.effective_bool() {
                                out.push_str(&format!("CONFIG_{}=y\n", sym.name));
                            } else {
                                out.push_str(&format!("# CONFIG_{} is not set\n", sym.name));
                            }
                        }
                        SymType::Int => {
                            out.push_str(&format!("CONFIG_{}={}\n", sym.name, sym.effective_int()));
                        }
                        SymType::Unknown => {}
                    }
                }
            }
            Item::Menu { items: sub, .. } => collect_config(sub, symbols, choices, out),
            Item::Choice(cname) => {
                if let Some(choice) = choices.get(cname) {
                    for n in &choice.option_names {
                        if let Some(sym) = symbols.get(n) {
                            if sym.effective_bool() {
                                out.push_str(&format!("CONFIG_{}=y\n", sym.name));
                            } else {
                                out.push_str(&format!("# CONFIG_{} is not set\n", sym.name));
                            }
                        }
                    }
                }
            }
        }
    }
}

pub fn write_autoconf(db: &KconfigDB, path: &Path) -> Result<(), std::io::Error> {
    if let Some(dir) = path.parent() {
        std::fs::create_dir_all(dir)?;
    }
    let mut out = String::from(
        "/* Generated by 3rd/tools/kconfig — do not edit. */\n\
         #ifndef BEDROCK_AUTOCONF_H\n\
         #define BEDROCK_AUTOCONF_H\n\n",
    );
    collect_autoconf(&db.items, &db.symbols, &db.choices, &mut out);
    out.push_str("\n#endif /* BEDROCK_AUTOCONF_H */\n");
    std::fs::write(path, out)
}

fn collect_autoconf(
    items: &[Item],
    symbols: &HashMap<String, Symbol>,
    choices: &HashMap<String, Choice>,
    out: &mut String,
) {
    for item in items {
        match item {
            Item::Sym(name) => {
                if let Some(sym) = symbols.get(name) {
                    match sym.typ {
                        SymType::Bool => {
                            if sym.effective_bool() {
                                out.push_str(&format!("#define CONFIG_{} 1\n", sym.name));
                            } else {
                                out.push_str(&format!("/* CONFIG_{} is not set */\n", sym.name));
                            }
                        }
                        SymType::Int => {
                            out.push_str(&format!(
                                "#define CONFIG_{} {}\n",
                                sym.name,
                                sym.effective_int()
                            ));
                        }
                        SymType::Unknown => {}
                    }
                }
            }
            Item::Menu { items: sub, .. } => collect_autoconf(sub, symbols, choices, out),
            Item::Choice(cname) => {
                if let Some(choice) = choices.get(cname) {
                    for n in &choice.option_names {
                        if let Some(sym) = symbols.get(n) {
                            if sym.effective_bool() {
                                out.push_str(&format!("#define CONFIG_{} 1\n", sym.name));
                            } else {
                                out.push_str(&format!("/* CONFIG_{} is not set */\n", sym.name));
                            }
                        }
                    }
                }
            }
        }
    }
}
