# Plugins

Install, inspect, update, and remove Waywallen plugins. This is a **popup**, not a primary nav tab.

## How it opens

From [`Window.qml`](../../../ui/qml/Window.qml) rail footer or Status actions:

```text
presentPopup('waywallen.ui/PagePopup', { source: 'waywallen.ui/PluginManagePage' })
```

## Main screens

| Role | File |
|------|------|
| Manage list | [`ui/qml/page/PluginManagePage.qml`](../../../ui/qml/page/PluginManagePage.qml) |
| Per-plugin settings | [`ui/qml/page/PluginSettingsPage.qml`](../../../ui/qml/page/PluginSettingsPage.qml) |

## Main methods and queries

| Name | Role |
|------|------|
| `PluginListQuery.reload()` | Lists installed plugins |
| `PluginInspectQuery` | Inspect a `.zip` before install |
| `PluginInstallQuery` | Install from package |
| `PluginDeleteQuery` | Remove a plugin |
| `PluginUpdateCheckQuery` | Check for updates |
| `PluginUpdateInstallQuery` | Install an update |

## Install / update decision flow

```mermaid
flowchart TD
  Open[presentPopup_PagePopup] --> Page[PluginManagePage]
  Page --> List[PluginListQuery]
  User[User_action] --> Kind{kind}
  Kind -->|install_zip| Inspect[PluginInspectQuery]
  Inspect -->|ok| Confirm[Install_dialog]
  Confirm --> Install[PluginInstallQuery]
  Inspect -->|fail| ToastFail[Error_toast]
  Kind -->|update_check| Check[PluginUpdateCheckQuery]
  Check --> Update[PluginUpdateInstallQuery]
  Kind -->|delete| Delete[PluginDeleteQuery]
  Install --> Daemon[waywallen_daemon]
  Update --> Daemon
  Delete --> Daemon
  Daemon --> RestartHint{needsRestart}
  RestartHint -->|yes| ToastRestart[Toast_restart_required]
  RestartHint -->|no| List
  ToastRestart --> List
```

See also: [open-wallpaper-engine](../../integrations/open-wallpaper-engine/README.md), [Status](../status/README.md).
