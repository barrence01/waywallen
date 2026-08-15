# Wallpapers

Browse the local wallpaper library, manage playlists, and apply wallpapers to displays.

## Main screens

| Role | File |
|------|------|
| Main page | [`ui/qml/page/WallpaperPage.qml`](../../../ui/qml/page/WallpaperPage.qml) |
| Detail / apply | [`ui/qml/page/WallpaperDetailPanel.qml`](../../../ui/qml/page/WallpaperDetailPanel.qml) |
| Card | [`ui/qml/page/WallpaperCard.qml`](../../../ui/qml/page/WallpaperCard.qml) |
| Info | [`ui/qml/page/WallpaperInfoPage.qml`](../../../ui/qml/page/WallpaperInfoPage.qml) |

Related: `AddLibraryPage.qml`, `SourceManagePage.qml`, sheets under `ui/qml/page/wallpaper/`.

## Main methods and queries

| Name | Role |
|------|------|
| `reloadAll()` | Reloads plugins, playlists, and filter settings |
| `applySort()` | Applies saved sort before first list load |
| `WallpaperListQuery` | Filtered/sorted library (reloads on filter/sort/search) |
| `WallpaperApplyQuery` | Applies wallpaper to selected displays (detail panel) |
| `PlaylistListQuery` | Playlist list / mutations |
| `WallpaperScanQuery` | Rescans library sources |
| `RendererPluginListQuery` | Renderer plugins for apply picker |

## Page load and list flow

```mermaid
flowchart TD
  Start[Component_onCompleted] --> Sort[applySort]
  Sort --> Ready{daemon_Ready}
  Ready -->|yes| Reload[reloadAll]
  Ready -->|no| Wait[Wait_Notify_daemonReady]
  Wait --> Reload
  Reload --> Plugins[RendererPluginListQuery]
  Reload --> Playlists[PlaylistListQuery]
  Reload --> Filters[SettingsGetQuery_filters]
  List[WallpaperListQuery] --> Grid[WallpaperCard_grid]
  FilterChange[User_filter_sort_search] --> List
```

## Apply decision flow

From `WallpaperDetailPanel`: user picks targets and optional renderer, then `WallpaperApplyQuery` runs.

```mermaid
flowchart TD
  Select[User_selects_wallpaper] --> Detail[WallpaperDetailPanel]
  Detail --> Targets{applyTargetIds}
  Targets -->|empty| AllDisplays[Apply_to_all_or_default]
  Targets -->|set| Specific[Apply_to_selected_displays]
  Detail --> RendererPick{user_picked_renderer}
  RendererPick -->|yes| Named[applyQuery.rendererName]
  RendererPick -->|no| Auto[Daemon_picks_by_wp_type]
  Named --> Apply[WallpaperApplyQuery.reload]
  Auto --> Apply
  AllDisplays --> Apply
  Specific --> Apply
  Apply --> Daemon[waywallen_daemon]
  Daemon --> Spawn[Spawn_or_reuse_renderer]
```

See also: [open-wallpaper-engine](../../integrations/open-wallpaper-engine/README.md), [Status](../status/README.md).
