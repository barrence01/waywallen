# Status

Daemon health, global mute/pause/stop, and live renderer list. Also provides shortcuts into Plugins, Settings, and About.

## Main screens

| Role | File |
|------|------|
| Main page | [`ui/qml/page/StatusPage.qml`](../../../ui/qml/page/StatusPage.qml) |

Live renderer rows also bind `W.App.rendererManager.renderers`.

## Main methods and queries

| Name | Role |
|------|------|
| `reloadAll()` | Reloads health, renderers, plugins, and settings |
| `HealthQuery` | Daemon health snapshot |
| `RendererListQuery` | Active renderer processes |
| `RendererPluginListQuery` | Installed renderer plugins |
| `SettingsGetQuery` | Relevant global settings |
| `GlobalMuteSetQuery` | Global mute |
| `GlobalPauseSetQuery` | Global pause |
| `GlobalStopSetQuery` | Global stop |
| `RendererKillQuery` | Kill a specific renderer |

## Load and control flow

```mermaid
flowchart TD
  Start[StatusPage_onCompleted] --> Ready{daemon_Ready}
  Ready -->|yes| Reload[reloadAll]
  Ready -->|no| Wait[Wait_Notify_daemonReady]
  Wait --> Reload
  Reload --> Health[HealthQuery]
  Reload --> RList[RendererListQuery]
  Reload --> Plugins[RendererPluginListQuery]
  Reload --> Settings[SettingsGetQuery]

  User[User_control] --> Kind{action}
  Kind -->|mute| Mute[GlobalMuteSetQuery]
  Kind -->|pause| Pause[GlobalPauseSetQuery]
  Kind -->|stop| Stop[GlobalStopSetQuery]
  Kind -->|kill_row| Kill[RendererKillQuery]
  Mute --> Daemon[waywallen_daemon]
  Pause --> Daemon
  Stop --> Daemon
  Kill --> Daemon
  Daemon --> Live[rendererManager_and_queries_refresh]
```

Footer / page actions can open Plugins or Settings via `Window.presentPopup` (same as the main rail).

See also: [Plugins](../plugins/README.md), [Settings](../settings/README.md).
