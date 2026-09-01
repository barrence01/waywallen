#include <QtTest/QtTest>

import waywallen;

using waywallen::RemoteSearchPageWindow;
using waywallen::ShareStore;
using waywallen::model::RemoteListModel;
using waywallen::model::RemoteRow;

namespace
{

auto makeRow(const QString& id) -> RemoteRow {
    RemoteRow row;
    row.sourceId = QStringLiteral("src");
    row.itemId   = id;
    row.title    = id;
    return row;
}

auto makeRows(char prefix, int count) -> QList<RemoteRow> {
    QList<RemoteRow> rows;
    rows.reserve(count);
    for (int i = 0; i < count; ++i)
        rows.push_back(makeRow(QStringLiteral("%1%2").arg(prefix).arg(i)));
    return rows;
}

struct ModelFixture {
    ShareStore<RemoteRow> store;
    RemoteListModel       model;

    ModelFixture() { model.set_store(&model, store); }
};

} // namespace

class RemoteSearchPageWindowTest : public QObject {
    Q_OBJECT

private slots:
    void reset_page_sets_offset_and_slice() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        const auto             result = window.applyPage(&fx.model,
                                                         1,
                                                         RemoteSearchPageWindow::FetchMode::Reset,
                                                         makeRows('a', 3),
                                                         true,
                                                         true,
                                                         0);
        QVERIFY(result.ok);
        QCOMPARE(result.offset, 0);
        QCOMPARE(result.noMore, false);
        QCOMPARE(window.slices.size(), 1);
        QCOMPARE(window.slices.front().page, 1u);
        QCOMPARE(fx.model.count(), 3);
        QVERIFY(fx.model.hasMore());
    }

    void append_grows_model_and_tracks_offset() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        qint32                 offset = 0;
        bool                   noMore = true;

        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2),
                               true,
                               true,
                               0)
                    .ok);
        for (quint32 page = 2; page <= 6; ++page) {
            const auto r = window.applyPage(&fx.model,
                                            page,
                                            RemoteSearchPageWindow::FetchMode::Append,
                                            makeRows(static_cast<char>('a' + page - 1), 2),
                                            true,
                                            noMore,
                                            offset);
            QVERIFY(r.ok);
            offset = r.offset;
            noMore = r.noMore;
        }
        QCOMPARE(fx.model.count(), 12);
        QCOMPARE(window.slices.size(), 6);
        QCOMPARE(window.slices.front().page, 1u);
        QCOMPARE(offset, 5);
    }

    void last_page_marks_no_more() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2),
                               true,
                               true,
                               0)
                    .ok);
        const auto last = window.applyPage(&fx.model,
                                           2,
                                           RemoteSearchPageWindow::FetchMode::Append,
                                           makeRows('b', 2),
                                           false,
                                           false,
                                           0);
        QVERIFY(last.ok);
        QVERIFY(last.noMore);
        QVERIFY(! fx.model.hasMore());
        QCOMPARE(fx.model.count(), 4);
    }

    void cache_hit_applies_without_duplicate() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;

        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2),
                               true,
                               true,
                               0)
                    .ok);
        window.putCache(2, makeRows('b', 2), true);

        const auto cached = window.tryApplyCached(
            &fx.model, 2, RemoteSearchPageWindow::FetchMode::Append, false, 0);
        QVERIFY(cached.ok);
        QCOMPARE(fx.model.count(), 4);
        QCOMPARE(window.slices.size(), 2);
        QVERIFY(! window.containsCache(2));

        const auto again = window.tryApplyCached(
            &fx.model, 2, RemoteSearchPageWindow::FetchMode::Append, false, 1);
        QVERIFY(! again.ok);
        QCOMPARE(fx.model.count(), 4);
    }

    void clear_empties_slices_and_cache() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 1),
                               true,
                               true,
                               0)
                    .ok);
        window.putCache(2, makeRows('b', 1), false);
        window.clear();
        QVERIFY(window.slicesEmpty());
        QVERIFY(! window.containsCache(2));
    }

    void page_applied_blocks_reapply() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2),
                               true,
                               true,
                               0)
                    .ok);
        QVERIFY(window.pageApplied(1));
        QVERIFY(! window.pageApplied(2));
        window.putCache(1, makeRows('x', 2), true);
        const auto dup =
            window.tryApplyCached(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset, false, 0);
        QVERIFY(! dup.ok);
    }

    void apply_page_null_model() {
        RemoteSearchPageWindow window;
        const auto             result = window.applyPage(
            nullptr, 1, RemoteSearchPageWindow::FetchMode::Reset, makeRows('a', 3), true, false, 0);
        QVERIFY(! result.ok);
        QVERIFY(window.slices.isEmpty());
    }

    void apply_page_empty_rows_does_not_add_slice() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;

        const auto resetResult = window.applyPage(
            &fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset, {}, true, false, 0);
        QVERIFY(resetResult.ok);
        QVERIFY(window.slices.isEmpty());
        QCOMPARE(fx.model.count(), 0);

        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2),
                               true,
                               false,
                               0)
                    .ok);
        QCOMPARE(window.slices.size(), 1);

        const auto appendResult = window.applyPage(
            &fx.model, 2, RemoteSearchPageWindow::FetchMode::Append, {}, true, false, 0);
        QVERIFY(appendResult.ok);
        QCOMPARE(window.slices.size(), 1);
        QCOMPARE(fx.model.count(), 2);
    }

    void try_apply_cached_append_blocked_when_no_base_slice() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        window.putCache(2, makeRows('b', 2), true);

        const auto result = window.tryApplyCached(
            &fx.model, 2, RemoteSearchPageWindow::FetchMode::Append, false, 0);
        QVERIFY(! result.ok);
        QVERIFY(window.containsCache(2));
        QCOMPARE(fx.model.count(), 0);
    }

    void try_apply_cached_append_blocked_when_no_more() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2),
                               false,
                               false,
                               0)
                    .ok);
        window.putCache(2, makeRows('b', 2), true);

        const auto result = window.tryApplyCached(&fx.model,
                                                  2,
                                                  RemoteSearchPageWindow::FetchMode::Append,
                                                  /*noMore=*/true,
                                                  0);
        QVERIFY(! result.ok);
        QVERIFY(window.containsCache(2));
        QCOMPARE(fx.model.count(), 2);
    }

    void try_apply_cached_reset_from_cache() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 4),
                               true,
                               false,
                               0)
                    .ok);

        window.putCache(3, makeRows('c', 3), true);

        const auto result =
            window.tryApplyCached(&fx.model, 3, RemoteSearchPageWindow::FetchMode::Reset, false, 0);
        QVERIFY(result.ok);
        QCOMPARE(fx.model.count(), 3);
        QVERIFY(! window.containsCache(3));
        QCOMPARE(window.slices.size(), 1);
        QCOMPARE(window.slices.front().page, 3u);
    }

    void apply_page_reset_clears_previous_slices() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        QVERIFY(window
                    .applyPage(&fx.model,
                               1,
                               RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2),
                               true,
                               false,
                               0)
                    .ok);
        QVERIFY(window
                    .applyPage(&fx.model,
                               2,
                               RemoteSearchPageWindow::FetchMode::Append,
                               makeRows('b', 2),
                               true,
                               false,
                               0)
                    .ok);
        QCOMPARE(window.slices.size(), 2);

        const auto reloadResult = window.applyPage(&fx.model,
                                                   1,
                                                   RemoteSearchPageWindow::FetchMode::Reset,
                                                   makeRows('x', 5),
                                                   true,
                                                   false,
                                                   0);
        QVERIFY(reloadResult.ok);
        QCOMPARE(window.slices.size(), 1);
        QCOMPARE(window.slices.front().page, 1u);
        QCOMPARE(fx.model.count(), 5);
    }

    void put_cache_overwrites_existing_entry() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        window.putCache(1, makeRows('a', 2), true);
        window.putCache(1, makeRows('b', 3), false);

        const auto result =
            window.tryApplyCached(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset, false, 0);
        QVERIFY(result.ok);
        QCOMPARE(fx.model.count(), 3);
        QVERIFY(result.noMore);
        QVERIFY(! window.containsCache(1));
    }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    int              status = 0;
    {
        RemoteSearchPageWindowTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    return status;
}

#include "discover_tests.moc"
