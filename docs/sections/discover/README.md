# Discover

Browse and download wallpapers from remote sources (plugin-backed remotes such as Workshop).

## Main screens

| Role | File |
|------|------|
| Main page | [`ui/qml/page/DiscoverPage.qml`](../../../ui/qml/page/DiscoverPage.qml) |
| Persisted UI state | [`ui/qml/page/DiscoverState.qml`](../../../ui/qml/page/DiscoverState.qml) |
| Card | [`ui/qml/page/RemoteCard.qml`](../../../ui/qml/page/RemoteCard.qml) |
| Detail | [`ui/qml/page/RemoteDetailPanel.qml`](../../../ui/qml/page/RemoteDetailPanel.qml) |
| Info / manage | `RemoteInfoPage.qml`, `RemoteManagePage.qml` |

## Main methods and queries

| Name | Role |
|------|------|
| `reloadAll()` | Reloads remote availability |
| `setSource(id)` | Switches remote; resets browse/sort/tags from `DiscoverState` |
| `pickSort(idx)` | Updates `RemoteSearchQuery.sortKey` |
| `RemoteAvailabilityQuery` | Which remotes are available / login state |
| `RemoteSearchQuery` | Search or browse results |
| `RemoteDetailsQuery` | Item details |
| `RemoteDownloadQuery` | Download into the library |
| `RemoteSubscriptionQuery` | Subscription management |

## Source and search flow

```mermaid
flowchart TD
  Load[DiscoverPage_onCompleted] --> Avail[RemoteAvailabilityQuery]
  Avail --> HasSrc{sources_available}
  HasSrc -->|no| Empty[Show_empty_or_login_hint]
  HasSrc -->|yes| SetSrc[setSource_selected_or_first]
  SetSrc --> CanBrowse{source_supports_browse}
  CanBrowse -->|yes| Browse[searchQuery_browsingEnabled]
  CanBrowse -->|no| NeedQuery[User_must_type_search]
  Browse --> Search[RemoteSearchQuery.reload]
  NeedQuery --> Search
  SortOrTags[pickSort_or_tag_change] --> Search
  Search --> Grid[RemoteCard_grid]
```

## Download path

```mermaid
flowchart TD
  Pick[User_opens_RemoteDetailPanel] --> Details[RemoteDetailsQuery]
  Details --> Action{user_action}
  Action -->|download| DL[RemoteDownloadQuery]
  Action -->|subscribe| Sub[RemoteSubscriptionQuery]
  DL --> Daemon[waywallen_daemon_plus_source_plugin]
  Sub --> Daemon
  Daemon --> Library[Local_library_updated]
  Library --> Wallpapers[Visible_in_Wallpapers_tab]
```

See also: [Wallpapers](../wallpapers/README.md), [Plugins](../plugins/README.md), [open-wallpaper-engine](../../integrations/open-wallpaper-engine/README.md).
