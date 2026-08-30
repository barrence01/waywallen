use std::fs;
use std::io::Write;
use std::path::Path;

use tracing_appender::rolling::{RollingFileAppender, Rotation};

fn count_daily_logs(dir: &Path) -> usize {
    fs::read_dir(dir)
        .unwrap()
        .flatten()
        .filter(|entry| {
            entry.file_type().is_ok_and(|ty| ty.is_file())
                && entry
                    .file_name()
                    .to_str()
                    .is_some_and(|name| name.starts_with("waywallen.") && name.ends_with(".log"))
        })
        .count()
}

#[test]
fn isolated_daily_directory_preserves_legacy_logs() {
    let dir = tempfile::tempdir().unwrap();
    let daily_dir = dir.path().join("daemon");
    fs::create_dir(&daily_dir).unwrap();
    for day in 1..=5 {
        fs::write(
            daily_dir.join(format!("waywallen.2000-01-{day:02}.log")),
            format!("old-{day}\n"),
        )
        .unwrap();
    }
    let legacy = dir.path().join("waywallen_r2026-08-26.log");
    let display = dir.path().join("waywallen_display_r2026-08-26.log");
    fs::write(&legacy, "legacy\n").unwrap();
    fs::write(&display, "display\n").unwrap();

    let mut appender = RollingFileAppender::builder()
        .rotation(Rotation::DAILY)
        .filename_prefix("waywallen")
        .filename_suffix("log")
        .max_log_files(3)
        .build(&daily_dir)
        .unwrap();
    writeln!(appender, "current").unwrap();
    appender.flush().unwrap();

    assert_eq!(count_daily_logs(&daily_dir), 3);
    assert_eq!(fs::read_to_string(legacy).unwrap(), "legacy\n");
    assert_eq!(fs::read_to_string(display).unwrap(), "display\n");
}
