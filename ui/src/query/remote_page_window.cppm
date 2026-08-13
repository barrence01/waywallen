module;
export module waywallen:query.remote_page_window;
export import :model.remote;

namespace waywallen
{

export struct RemoteSearchPageWindow {
    enum class FetchMode { Reset, Append };

    struct PageSlice {
        quint32 page { 0 };
        int     count { 0 };
    };

    struct CachedPage {
        QList<model::RemoteRow> rows;
        bool                    hasMore { false };
    };

    struct ApplyResult {
        bool   ok { false };
        qint32 offset { 0 };
        bool   noMore { false };
    };

    void clear();
    auto pageApplied(quint32 page) const -> bool;
    auto containsCache(quint32 page) const -> bool;
    void putCache(quint32 page, const QList<model::RemoteRow>& rows, bool hasMore);
    auto slicesEmpty() const -> bool;

    auto applyPage(model::RemoteListModel* model, quint32 page, FetchMode mode,
                   const QList<model::RemoteRow>& rows, bool more, bool noMore,
                   qint32 offset) -> ApplyResult;
    auto tryApplyCached(model::RemoteListModel* model, quint32 page, FetchMode mode, bool noMore,
                        qint32 offset) -> ApplyResult;

    QList<PageSlice>           slices;
    QHash<quint32, CachedPage> cache;
};

} // namespace waywallen
