module;
module waywallen;
import :query.remote_page_window;

namespace waywallen
{

void RemoteSearchPageWindow::clear() {
    slices.clear();
    cache.clear();
}

auto RemoteSearchPageWindow::hasPrevious() const -> bool {
    return ! slices.isEmpty() && slices.front().page > 1;
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

auto RemoteSearchPageWindow::frontPage() const -> quint32 {
    return slices.isEmpty() ? 0 : slices.front().page;
}

auto RemoteSearchPageWindow::slicesEmpty() const -> bool { return slices.isEmpty(); }

auto RemoteSearchPageWindow::enforceWindow(model::RemoteListModel* t, TrimSide side,
                                           ApplyResult& result) -> int {
    if (! t) return 0;

    int removed_leading = 0;
    while (slices.size() > kMaxHotPages) {
        if (side == TrimSide::Front) {
            const auto slice = slices.takeFirst();
            t->trimFront(slice.count);
            removed_leading += slice.count;
        } else {
            const auto slice = slices.takeLast();
            t->trimBack(slice.count);
            result.noMore = false;
            t->setHasMore(true);
        }
    }
    if (! slices.isEmpty()) result.offset = static_cast<qint32>(slices.back().page - 1);
    return removed_leading;
}

auto RemoteSearchPageWindow::applyPage(model::RemoteListModel* t, quint32 page, FetchMode mode,
                                       const QList<model::RemoteRow>& rows, bool more, bool noMore,
                                       qint32 offset) -> ApplyResult {
    ApplyResult result;
    result.offset = offset;
    result.noMore = noMore;
    if (! t) return result;

    if (mode == FetchMode::Reset) {
        slices.clear();
        if (! rows.isEmpty()) slices.push_back({ page, static_cast<int>(rows.size()) });
        result.offset = static_cast<qint32>(page - 1);
        result.noMore = ! more;
        t->reset(rows, more);
    } else if (mode == FetchMode::Append) {
        if (! rows.isEmpty()) slices.push_back({ page, static_cast<int>(rows.size()) });
        result.offset = static_cast<qint32>(page - 1);
        result.noMore = ! more;
        t->append(rows, more);
        const int trimmed = enforceWindow(t, TrimSide::Front, result);
        if (trimmed) result.leadingDelta = -trimmed;
    } else {
        if (! rows.isEmpty()) {
            slices.prepend({ page, static_cast<int>(rows.size()) });
            t->prepend(rows);
            result.leadingDelta = static_cast<int>(rows.size());
            enforceWindow(t, TrimSide::Back, result);
        }
        t->setHasMore(! result.noMore);
    }
    result.ok = true;
    return result;
}

auto RemoteSearchPageWindow::tryApplyCached(model::RemoteListModel* t, quint32 page, FetchMode mode,
                                            bool noMore, qint32 offset) -> ApplyResult {
    if (pageApplied(page)) return {};
    const auto it = cache.constFind(page);
    if (it == cache.cend()) return {};
    if (mode == FetchMode::Append) {
        if (slices.isEmpty() || noMore) return {};
    }
    return applyPage(t, page, mode, it->rows, it->hasMore, noMore, offset);
}

} // namespace waywallen
