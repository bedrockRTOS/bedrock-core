// SPDX-License-Identifier: Unlicense
//
// Interactive TUI for editing kernel configuration.
// Keys: ↑↓/k/j — navigate, Enter/Space — toggle or edit,
//       → or Enter on menu — enter submenu, ←/Esc — go back,
//       S — save, Q — save and quit, X — quit without saving.

use crossterm::{
    cursor,
    event::{self, Event, KeyCode, KeyEvent},
    execute, queue,
    style::{Attribute, Color, Print, ResetColor, SetAttribute, SetForegroundColor},
    terminal::{self, ClearType, EnterAlternateScreen, LeaveAlternateScreen},
};
use kconfig_tools::{self as kc, Item, Items, KconfigDB};
use std::io::{self, Write};
use std::path::PathBuf;
use std::rc::Rc;

struct NavFrame {
    items: Items,
    title: String,
    in_choice: Option<String>,
}

fn flag(args: &[String], name: &str, default: &str) -> String {
    let mut it = args.iter();
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

    let mut db = kc::parse(&kconfig).unwrap_or_else(|e| {
        eprintln!("error: cannot parse {}: {e}", kconfig.display());
        std::process::exit(1);
    });
    kc::load_dot_config(&mut db, &config);

    if !is_tty() {
        eprintln!("menuconfig: stdin is not a terminal");
        std::process::exit(1);
    }

    let saved = run(&mut db);

    if saved {
        kc::save_dot_config(&db, &config).unwrap_or_else(|e| {
            eprintln!("error: {e}");
            std::process::exit(1);
        });
        kc::write_autoconf(&db, &header).unwrap_or_else(|e| {
            eprintln!("error: {e}");
            std::process::exit(1);
        });
        println!("Saved  {}", config.display());
        println!("  GEN  {}", header.display());
    } else {
        println!("Quit without saving.");
    }
}

fn is_tty() -> bool {
    use std::io::IsTerminal;
    std::io::stdin().is_terminal()
}

fn run(db: &mut KconfigDB) -> bool {
    let mut out = io::stdout();
    terminal::enable_raw_mode().expect("failed to enable raw mode");
    execute!(out, EnterAlternateScreen, cursor::Hide).unwrap();

    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| event_loop(db, &mut out)));

    execute!(out, LeaveAlternateScreen, cursor::Show).unwrap();
    terminal::disable_raw_mode().expect("failed to disable raw mode");

    result.unwrap_or(false)
}

fn event_loop(db: &mut KconfigDB, out: &mut impl Write) -> bool {
    let mut nav: Vec<NavFrame> = vec![NavFrame {
        items: db.items.clone(),
        title: db.mainmenu.clone(),
        in_choice: None,
    }];
    let mut cursor = 0usize;
    let mut scroll = 0usize;
    let mut saved = false;
    let mut flash: Option<String> = None;

    loop {
        let (cols, rows) = terminal::size().unwrap_or((80, 24));
        let (cols, rows) = (cols as usize, rows as usize);
        let list_h = rows.saturating_sub(3);

        let n_items = nav.last().unwrap().items.len();
        if n_items > 0 {
            cursor = cursor.min(n_items - 1);
        }
        if cursor < scroll {
            scroll = cursor;
        } else if list_h > 0 && cursor >= scroll + list_h {
            scroll = cursor + 1 - list_h;
        }

        draw(out, &nav, db, cursor, scroll, list_h, cols, rows, flash.as_deref());
        flash = None;

        let key = loop {
            match event::read().unwrap() {
                Event::Key(k) if k.kind == event::KeyEventKind::Press => break k,
                Event::Resize(..) => {
                    draw(out, &nav, db, cursor, scroll, list_h, cols, rows, None);
                }
                _ => {}
            }
        };

        let in_choice = nav.last().unwrap().in_choice.clone();
        let item = nav.last().unwrap().items.get(cursor).cloned();

        match key.code {
            KeyCode::Up | KeyCode::Char('k') => {
                cursor = cursor.saturating_sub(1);
            }
            KeyCode::Down | KeyCode::Char('j') => {
                if n_items > 0 {
                    cursor = (cursor + 1).min(n_items - 1);
                }
            }
            KeyCode::Char('q') | KeyCode::Char('Q') => {
                saved = true;
                break;
            }
            KeyCode::Char('x') | KeyCode::Char('X') => {
                saved = false;
                break;
            }
            KeyCode::Char('s') | KeyCode::Char('S') => {
                saved = true;
                flash = Some("Saved.".to_owned());
            }
            KeyCode::Esc | KeyCode::Left => {
                if nav.len() > 1 {
                    nav.pop();
                    cursor = 0;
                    scroll = 0;
                } else {
                    saved = false;
                    break;
                }
            }
            KeyCode::Enter | KeyCode::Char(' ') | KeyCode::Right => {
                let Some(item) = item else { continue };
                handle_activate(item, key.code, &in_choice, &mut nav, db, out, rows, cols,
                                &mut cursor, &mut scroll, &mut saved);
            }
            _ => {}
        }
    }

    saved
}

fn handle_activate(
    item: Item,
    key: KeyCode,
    in_choice: &Option<String>,
    nav: &mut Vec<NavFrame>,
    db: &mut KconfigDB,
    out: &mut impl Write,
    rows: usize,
    cols: usize,
    cursor: &mut usize,
    scroll: &mut usize,
    saved: &mut bool,
) {
    match item {
        Item::Menu { title, items } => {
            if key != KeyCode::Char(' ') {
                nav.push(NavFrame { items, title, in_choice: None });
                *cursor = 0;
                *scroll = 0;
            }
        }
        Item::Choice(cname) => {
            if key != KeyCode::Char(' ') {
                if let Some(choice) = db.choices.get(&cname) {
                    let opt_items: Vec<Item> =
                        choice.option_names.iter().map(|n| Item::Sym(n.clone())).collect();
                    nav.push(NavFrame {
                        items: Rc::new(opt_items),
                        title: choice.prompt.clone(),
                        in_choice: Some(cname),
                    });
                    *cursor = 0;
                    *scroll = 0;
                }
            }
        }
        Item::Sym(name) => {
            if key == KeyCode::Right {
                return;
            }
            if let Some(cname) = in_choice {
                if let Some(choice) = db.choices.get(cname) {
                    let opts: Vec<String> = choice.option_names.clone();
                    for n in &opts {
                        if let Some(sym) = db.symbols.get_mut(n) {
                            sym.value = Some(kc::Value::Bool(*n == name));
                        }
                    }
                    *saved = true;
                }
            } else if let Some(sym) = db.symbols.get(&name) {
                match sym.typ {
                    kc::SymType::Bool => {
                        let v = sym.effective_bool();
                        db.symbols.get_mut(&name).unwrap().value = Some(kc::Value::Bool(!v));
                        *saved = true;
                    }
                    kc::SymType::Int => {
                        if let Some(v) = edit_int(out, &db.symbols[&name], rows, cols) {
                            db.symbols.get_mut(&name).unwrap().value = Some(kc::Value::Int(v));
                            *saved = true;
                        }
                    }
                    kc::SymType::Unknown => {}
                }
            }
        }
    }
}

fn draw(
    out: &mut impl Write,
    nav: &[NavFrame],
    db: &KconfigDB,
    cursor: usize,
    scroll: usize,
    list_h: usize,
    cols: usize,
    rows: usize,
    flash: Option<&str>,
) {
    let frame = nav.last().unwrap();
    let items = &frame.items;
    let title = &frame.title;
    let in_choice = &frame.in_choice;

    queue!(out, terminal::Clear(ClearType::All)).unwrap();

    let hdr = center(&format!(" {title} "), cols);
    queue!(
        out,
        cursor::MoveTo(0, 0),
        SetAttribute(Attribute::Bold),
        SetForegroundColor(Color::Cyan),
        Print(&hdr),
        ResetColor,
        SetAttribute(Attribute::Reset),
    )
    .unwrap();

    for rel in 0..list_h {
        let real = scroll + rel;
        let row = (rel + 1) as u16;
        queue!(out, cursor::MoveTo(0, row), terminal::Clear(ClearType::CurrentLine)).unwrap();
        let Some(item) = items.get(real) else { continue };
        let line = fmt_item(item, in_choice, db, cols - 2);
        if real == cursor {
            queue!(
                out,
                cursor::MoveTo(1, row),
                SetAttribute(Attribute::Reverse),
                Print(pad(&line, cols - 2)),
                SetAttribute(Attribute::Reset),
            )
            .unwrap();
        } else {
            queue!(out, cursor::MoveTo(1, row), Print(&line)).unwrap();
        }
    }

    let info = flash
        .map(|s| s.to_owned())
        .or_else(|| hint(items.get(cursor), in_choice, db, cols - 2));
    queue!(
        out,
        cursor::MoveTo(0, (rows - 2) as u16),
        terminal::Clear(ClearType::CurrentLine),
    )
    .unwrap();
    if let Some(info) = info {
        queue!(
            out,
            cursor::MoveTo(1, (rows - 2) as u16),
            SetAttribute(Attribute::Dim),
            Print(truncate(&info, cols - 2)),
            SetAttribute(Attribute::Reset),
        )
        .unwrap();
    }

    let footer = " Spc/Enter:toggle  ←/Esc:back  S:save  Q:quit  X:no-save ";
    queue!(
        out,
        cursor::MoveTo(0, (rows - 1) as u16),
        terminal::Clear(ClearType::CurrentLine),
        SetAttribute(Attribute::Bold),
        SetForegroundColor(Color::Cyan),
        Print(truncate(footer, cols - 1)),
        ResetColor,
        SetAttribute(Attribute::Reset),
    )
    .unwrap();

    out.flush().unwrap();
}

fn fmt_item(item: &Item, in_choice: &Option<String>, db: &KconfigDB, width: usize) -> String {
    match item {
        Item::Sym(name) => {
            let Some(sym) = db.symbols.get(name) else {
                return name.clone();
            };
            let label = if sym.prompt.is_empty() { &sym.name } else { &sym.prompt };
            if let Some(cname) = in_choice {
                let choice = db.choices.get(cname);
                let selected = choice
                    .and_then(|c| c.selected(&db.symbols))
                    .map(|s| s.name == sym.name)
                    .unwrap_or(false);
                let mark = if selected { "(*) " } else { "( ) " };
                return truncate(&format!("{mark}{label}"), width);
            }
            match sym.typ {
                kc::SymType::Bool => {
                    let mark = if sym.effective_bool() { "[*] " } else { "[ ] " };
                    truncate(&format!("{mark}{label}"), width)
                }
                kc::SymType::Int => truncate(&format!("({}) {label}", sym.effective_int()), width),
                kc::SymType::Unknown => truncate(label, width),
            }
        }
        Item::Menu { title, .. } => truncate(&format!("    {title}  --->"), width),
        Item::Choice(cname) => {
            let choice = db.choices.get(cname);
            let prompt = choice.map(|c| c.prompt.as_str()).unwrap_or(cname);
            let sel = choice
                .and_then(|c| c.selected(&db.symbols))
                .map(|s| if s.prompt.is_empty() { s.name.as_str() } else { s.prompt.as_str() })
                .unwrap_or("?");
            truncate(&format!("    {prompt}: {sel}  --->"), width)
        }
    }
}

fn hint(item: Option<&Item>, _in_choice: &Option<String>, db: &KconfigDB, width: usize) -> Option<String> {
    let item = item?;
    match item {
        Item::Sym(name) => {
            let sym = db.symbols.get(name)?;
            if sym.help.is_empty() {
                None
            } else {
                Some(truncate(sym.help.lines().next().unwrap_or(""), width))
            }
        }
        Item::Menu { title, .. } => Some(truncate(&format!("Submenu: {title}"), width)),
        Item::Choice(cname) => {
            let prompt = db.choices.get(cname).map(|c| c.prompt.as_str()).unwrap_or(cname);
            Some(truncate(&format!("Choice: {prompt}"), width))
        }
    }
}

fn edit_int(out: &mut impl Write, sym: &kc::Symbol, rows: usize, cols: usize) -> Option<i64> {
    let label = if sym.prompt.is_empty() { &sym.name } else { &sym.prompt };
    let range_str = sym
        .range
        .map(|(lo, hi)| format!("  [{lo}..{hi}]"))
        .unwrap_or_default();
    let prompt = format!("  {label}{range_str}: ");

    let mut buf: Vec<char> = sym.effective_int().to_string().chars().collect();

    loop {
        let display: String = buf.iter().collect();
        let line = format!("{prompt}{display} ");
        queue!(
            out,
            cursor::MoveTo(0, (rows - 2) as u16),
            terminal::Clear(ClearType::CurrentLine),
            SetForegroundColor(Color::Yellow),
            Print(truncate(&line, cols - 1)),
            ResetColor,
            cursor::Show,
        )
        .unwrap();
        out.flush().unwrap();

        match event::read().unwrap() {
            Event::Key(KeyEvent { code, kind: event::KeyEventKind::Press, .. }) => match code {
                KeyCode::Enter => {
                    let _ = queue!(out, cursor::Hide);
                    let s: String = buf.iter().collect();
                    return s.parse::<i64>().ok().map(|v| {
                        if let Some((lo, hi)) = sym.range {
                            v.max(lo).min(hi)
                        } else {
                            v
                        }
                    });
                }
                KeyCode::Esc => {
                    let _ = queue!(out, cursor::Hide);
                    return None;
                }
                KeyCode::Backspace => {
                    buf.pop();
                }
                KeyCode::Char(c) if c.is_ascii_digit() => buf.push(c),
                KeyCode::Char('-') if buf.is_empty() => buf.push('-'),
                _ => {}
            },
            _ => {}
        }
    }
}

fn center(s: &str, width: usize) -> String {
    let len = s.chars().count();
    if len >= width {
        return s[..width].to_owned();
    }
    let pad = (width - len) / 2;
    format!("{:pad$}{s}", "")
}

fn pad(s: &str, width: usize) -> String {
    let len = s.chars().count();
    if len >= width {
        s[..width].to_owned()
    } else {
        format!("{s}{:width$}", "", width = width - len)
    }
}

fn truncate(s: &str, width: usize) -> String {
    let v: Vec<char> = s.chars().collect();
    if v.len() <= width {
        s.to_owned()
    } else {
        v[..width].iter().collect()
    }
}
