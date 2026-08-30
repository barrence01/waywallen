# Logging

Waywallen writes daily daemon logs under:

- `$XDG_STATE_HOME/waywallen/logs/` when `XDG_STATE_HOME` is set
- otherwise `~/.local/state/waywallen/logs/`

When neither `XDG_STATE_HOME` nor `HOME` is set, the daemon logs to stderr only.

Daemon logs use the `daemon/` subdirectory so their retention policy cannot remove legacy daemon or display logs. Files use UTC daily names such as `daemon/waywallen.2026-08-30.log`; `daemon/waywallen-current.log` points to the active file. At most eight daily files are retained.

File output has no ANSI escapes, while stderr uses color when connected to a terminal. File writes use a bounded lossy queue so a blocked disk cannot stall the daemon. The queue is flushed on normal shutdown; if it overflows, the dropped-line count is reported directly to stderr.

## Debug logging setting

In **Settings**, enable **Debug logging** to raise the daemon filter from the default (`info,zbus=warn`) to `debug,zbus=warn`. The change applies immediately to both the log file and stderr.

When `WW_LOG` is set in the daemon environment, the Settings toggle is disabled and shows *Disabled because WW_LOG is set.* The saved debug preference is not modified; removing `WW_LOG` and restarting restores the saved setting.

Use **Log folder → Open** in Settings to open the log directory in the file manager.

## Environment overrides

When `WW_LOG` is set, it overrides the Debug logging setting for the daemon and renderer processes:

```bash
export WW_LOG=debug
./waywallen
```

For the daemon, `WW_LOG` accepts the same `tracing-subscriber` filter syntax as `RUST_LOG`, such as `debug,waywallen::probe=trace`. The daemon always appends `zbus=warn`, overriding any `zbus` directive supplied by the environment.

When the filter starts with a global `off`, `error`, `warn`, `info`, `debug`, or `trace` directive, the daemon passes that level through `WW_LOG` when spawning a renderer. A target-only filter leaves renderers at `info`. Later Debug logging changes are sent to running renderers through renderer IPC.

## Child process stderr

Renderer and display child stderr is forwarded to the daemon log at **info** level with target `waywallen::child`, prefixed by role. Severity is not inferred from message text. Supervisors retain a bounded tail only for concise process-failure diagnostics.

## Changing the default filter in code

There is no build-time flag (Cargo feature / AppImage script) for the default level. Edit `DEFAULT_FILTER` and `DEBUG_FILTER` in `src/logging/policy.rs` and rebuild. `DEFAULT_FILTER` is used at daemon init and whenever Debug logging is off (unless `WW_LOG` is set). The Debug logging setting uses `filter_for_debug(true)` → `DEBUG_FILTER`.
