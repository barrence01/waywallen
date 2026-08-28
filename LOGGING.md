# Logging

Waywallen writes daily daemon logs under:

- `$XDG_STATE_HOME/waywallen/logs/` when `XDG_STATE_HOME` is set
- otherwise `~/.local/state/waywallen/logs/`

When neither `XDG_STATE_HOME` nor `HOME` is set, the daemon logs to stderr only.

The active daemon log is `waywallen_rCURRENT.log`, appended across restarts. On daily rotation it becomes `waywallen_rYYYY-MM-DD_00-00-00.log` (the `_00-00-00` suffix is a flexi_logger compatibility shim, not a real timestamp). flexi_logger 0.31 `Cleanup::KeepLogFiles(7)` keeps at most the seven newest rotated log files and deletes older ones.
Each log line uses flexi_logger `opt_format` (timestamp + level + file:line). Stderr uses the colored variant of the same format. File I/O is asynchronous and flushed about every 2 seconds, so lines may appear in the log file with a short delay.

## Debug logging setting

In **Settings**, enable **Debug logging** to raise the daemon filter from the default (`info,zbus=warn`) to `debug,zbus=warn`. The change applies immediately to both the log file and stderr.

When `RUST_LOG` is set in the daemon environment, the Settings toggle is disabled and shows *Disabled because RUST_LOG is set.* The saved debug preference is not modified; removing `RUST_LOG` and restarting restores the saved setting.

Use **Log folder → Open** in Settings to open the log directory in the file manager.

## Environment overrides

When `RUST_LOG` is set, it overrides the Debug logging setting for the daemon filter:

```bash
export RSTD_LOG=debug RUST_LOG=debug,zbus=warn
./waywallen
```

- `RUST_LOG` — Rust/`flexi_logger` filter for the daemon
- `RSTD_LOG` — C++/`rstd` plugin loggers (not controlled by the Debug logging setting)

## Child process stderr

Renderer and display child stderr is forwarded to the daemon log at **debug** level with target `waywallen::child`, prefixed by role (`[renderer]` / `[display]`). Severity is not inferred from message text.

## Changing the default filter in code

There is no build-time flag (Cargo feature / AppImage script) for the default level. Edit `DEFAULT_FILTER` and `DEBUG_FILTER` in `src/logging/policy.rs` and rebuild. `DEFAULT_FILTER` is used at daemon init and whenever Debug logging is off (unless `RUST_LOG` is set). The Debug logging setting uses `filter_for_debug(true)` → `DEBUG_FILTER`.
