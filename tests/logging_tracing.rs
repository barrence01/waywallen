use std::fs;

use waywallen::logging::{self, LoggingPolicy};

#[test]
fn log_and_tracing_share_the_reloadable_file_pipeline() {
    let dir = tempfile::tempdir().unwrap();
    let guard = logging::init_in(LoggingPolicy::DEFAULT, dir.path());

    log::info!(target: "logging_test", "log-facade-message");
    tracing::info!(target: "logging_test", "tracing-message");
    log::debug!(target: "logging_test", "debug-before-reload");
    logging::apply_debug_setting(true);
    log::debug!(target: "logging_test", "debug-after-reload");
    drop(guard);

    let contents = fs::read_to_string(dir.path().join("daemon/waywallen-current.log")).unwrap();
    assert_eq!(contents.matches("log-facade-message").count(), 1);
    assert_eq!(contents.matches("tracing-message").count(), 1);
    assert!(!contents.contains("debug-before-reload"));
    assert!(contents.contains("debug-after-reload"));
    assert!(!contents.contains("\u{1b}["));
}
