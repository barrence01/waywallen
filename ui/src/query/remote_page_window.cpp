module;
module waywallen;
import :query.remote_page_window;

namespace waywallen
{

void RemoteSearchPageWindow::clear() {
    slices.clear();
    cache.clear();
}

auto RemoteSearchPageWindow::pageApplied(quint32 page) const -> bool {
    for (const auto& slice : slices) {
        if (slice.page == page) return true;
    }
    return false;
}

auto RemoteSearchPageWindow::containsCache(quint32 page) const -> bool {
    return cache.contains(page);
}

void RemoteSearchPageWindow::putCache(quint32 page, const QList<model::RemoteRow>& rows,
                                      bool hasMore) {
    cache.insert(page, CachedPage { rows, hasMore });
}

auto RemoteSearchPageWindow::slicesEmpty() const -> bool { return slices.isEmpty(); }

auto RemoteSearchPageWindow::applyPage(model::RemoteListModel* t, quint32 page, FetchMode mode,
                                       const QList<model::RemoteRow>& rows, bool more, bool noMore,
                                       qint32 offset) -> ApplyResult {
    ApplyResult result;
    result.offset = offset;
    result.noMore = noMore;
    if (! t) return result;

    if (mode == FetchMode::Reset) {
        slices.clear();
        t->reset(rows, more);
    } else {
        t->append(rows, more);
    }
    if (! rows.isEmpty()) slices.push_back({ page, static_cast<int>(rows.size()) });
    result.offset = static_cast<qint32>(page - 1);
    result.noMore = ! more;
    result.ok     = true;
    return result;
}

auto RemoteSearchPageWindow::tryApplyCached(model::RemoteListModel* t, quint32 page, FetchMode mode,
                                            bool noMore, qint32 offset) -> ApplyResult {
    if (pageApplied(page)) return {};
    const auto it = cache.constFind(page);
    if (it == cache.cend()) return {};
    if (mode == FetchMode::Append && (slices.isEmpty() || noMore)) return {};

    const auto result = applyPage(t, page, mode, it->rows, it->hasMore, noMore, offset);
    if (result.ok) cache.remove(page);
    return result;
}

} // namespace waywallen
