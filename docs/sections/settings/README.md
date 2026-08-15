# Settings

Global application settings (playback defaults, cache, autostart, and related options). This is a **popup**, not a primary nav tab.

## How it opens

From [`Window.qml`](../../../ui/qml/Window.qml) rail footer or Status actions:

```text
presentPopup('waywallen.ui/PagePopup', { source: 'waywallen.ui/SettingsPage' })
```

## Main screens

| Role | File |
|------|------|
| Settings page | [`ui/qml/page/SettingsPage.qml`](../../../ui/qml/page/SettingsPage.qml) |

## Main methods and queries

| Name | Role |
|------|------|
| `SettingsGetQuery` | Load current global settings |
| `SettingsSetQuery` | Persist setting changes |
| `AutostartGetQuery` / `AutostartSetQuery` | Flatpak login autostart only |
| `resetSettings()` | Restore defaults (when exposed in UI) |
| `App.refreshNetworkCacheSize()` | Refresh network cache size display |

## Load and save flow

```mermaid
flowchart TD
  Open[presentPopup_PagePopup] --> Page[SettingsPage]
  Page --> Cache[App.refreshNetworkCacheSize]
  Page --> Ready{daemon_Ready}
  Ready -->|yes| Get[SettingsGetQuery]
  Ready -->|no| Wait[Wait_Notify_daemonReady]
  Wait --> Get
  Get --> Flatpak{isFlatpak}
  Flatpak -->|yes| AutoGet[AutostartGetQuery]
  Flatpak -->|no| Form[Settings_form]
  AutoGet --> Form

  Edit[User_changes_value] --> Set[SettingsSetQuery]
  Set --> Daemon[waywallen_daemon]
  Daemon -->|settingsChanged| Get
  AutoEdit[User_toggles_autostart] --> AutoSet[AutostartSetQuery]
  AutoSet --> AutoGet
```

See also: [Status](../status/README.md), [Plugins](../plugins/README.md).
