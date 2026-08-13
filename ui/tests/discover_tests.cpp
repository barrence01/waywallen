#include <QtTest/QtTest>

import waywallen;

using waywallen::RemoteSearchPageWindow;
using waywallen::ShareStore;
using waywallen::model::RemoteListModel;
using waywallen::model::RemoteRow;

namespace {

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

class RemoteListModelTest : public QObject {
    Q_OBJECT

private slots:
    void prepend_inserts_at_front() {
        ModelFixture fx;
        fx.model.reset(makeRows('a', 2), true);
        fx.model.prepend(makeRows('b', 2));
        QCOMPARE(fx.model.count(), 4);
        QCOMPARE(fx.model.itemIds(),
                 (QStringList { QStringLiteral("b0"), QStringLiteral("b1"), QStringLiteral("a0"),
                                QStringLiteral("a1") }));
    }

    void prepend_empty_is_noop() {
        ModelFixture fx;
        fx.model.reset(makeRows('a', 1), false);
        fx.model.prepend({});
        QCOMPARE(fx.model.count(), 1);
        QCOMPARE(fx.model.itemIds(), (QStringList { QStringLiteral("a0") }));
    }

    void trimFront_removes_leading_rows() {
        ModelFixture fx;
        fx.model.reset(makeRows('a', 4), true);
        fx.model.trimFront(2);
        QCOMPARE(fx.model.count(), 2);
        QCOMPARE(fx.model.itemIds(),
                 (QStringList { QStringLiteral("a2"), QStringLiteral("a3") }));
    }

    void trimBack_removes_trailing_rows() {
        ModelFixture fx;
        fx.model.reset(makeRows('a', 4), true);
        fx.model.trimBack(2);
        QCOMPARE(fx.model.count(), 2);
        QCOMPARE(fx.model.itemIds(),
                 (QStringList { QStringLiteral("a0"), QStringLiteral("a1") }));
    }

    void trim_non_positive_is_noop() {
        ModelFixture fx;
        fx.model.reset(makeRows('a', 2), true);
        fx.model.trimFront(0);
        fx.model.trimFront(-1);
        fx.model.trimBack(0);
        fx.model.trimBack(-3);
        QCOMPARE(fx.model.count(), 2);
    }
};

class RemoteSearchPageWindowTest : public QObject {
    Q_OBJECT

private slots:
    void reset_page_one_has_no_previous() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        const auto result = window.applyPage(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset,
                                             makeRows('a', 3), true, true, 0);
        QVERIFY(result.ok);
        QCOMPARE(result.offset, 0);
        QCOMPARE(result.noMore, false);
        QCOMPARE(result.leadingDelta, 0);
        QVERIFY(! window.hasPrevious());
        QCOMPARE(window.slices.size(), 1);
        QCOMPARE(window.slices.front().page, 1u);
        QCOMPARE(fx.model.count(), 3);
        QVERIFY(fx.model.hasMore());
    }

    void append_past_hot_window_trims_front() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        qint32                 offset = 0;
        bool                   noMore = true;

        auto apply = [&](quint32 page, char prefix) -> RemoteSearchPageWindow::ApplyResult {
            const auto r =
                window.applyPage(&fx.model, page, RemoteSearchPageWindow::FetchMode::Append,
                                 makeRows(prefix, 2), true, noMore, offset);
            offset = r.offset;
            noMore = r.noMore;
            return r;
        };

        QVERIFY(window
                    .applyPage(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2), true, true, 0)
                    .ok);

        // Pages 2..6 fill the hot window (6 pages). Page 7 must trim page 1.
        for (quint32 page = 2; page <= RemoteSearchPageWindow::kMaxHotPages; ++page) {
            const auto r = apply(page, static_cast<char>('a' + page - 1));
            QVERIFY(r.ok);
        }

        QCOMPARE(window.slices.size(), RemoteSearchPageWindow::kMaxHotPages);
        const auto trimmed = apply(RemoteSearchPageWindow::kMaxHotPages + 1, 'z');
        QVERIFY(trimmed.ok);
        QCOMPARE(trimmed.leadingDelta, -2);
        QCOMPARE(window.slices.size(), RemoteSearchPageWindow::kMaxHotPages);
        QCOMPARE(window.slices.front().page, 2u);
        QVERIFY(window.hasPrevious());
        QCOMPARE(offset, static_cast<qint32>(RemoteSearchPageWindow::kMaxHotPages));
        QCOMPARE(fx.model.count(), RemoteSearchPageWindow::kMaxHotPages * 2);
    }

    void prepend_restores_has_more_when_trimming_back() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        qint32                 offset = 0;
        bool                   noMore = false;

        // Window starts at page 2 so hasPrevious is true once filled.
        QVERIFY(window
                    .applyPage(&fx.model, 2, RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('b', 2), true, true, 0)
                    .ok);
        for (quint32 page = 3; page <= RemoteSearchPageWindow::kMaxHotPages + 1; ++page) {
            const auto r =
                window.applyPage(&fx.model, page, RemoteSearchPageWindow::FetchMode::Append,
                                 makeRows(static_cast<char>('a' + page - 1), 2), true, noMore,
                                 offset);
            QVERIFY(r.ok);
            offset = r.offset;
            noMore = r.noMore;
        }
        QCOMPARE(window.slices.size(), RemoteSearchPageWindow::kMaxHotPages);
        QVERIFY(window.hasPrevious());

        noMore = true;
        fx.model.setHasMore(false);

        const auto prepended =
            window.applyPage(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Prepend,
                             makeRows('p', 2), true, noMore, offset);
        QVERIFY(prepended.ok);
        QCOMPARE(prepended.leadingDelta, 2);
        QCOMPARE(prepended.noMore, false);
        QVERIFY(fx.model.hasMore());
        QCOMPARE(window.slices.front().page, 1u);
        QCOMPARE(window.slices.size(), RemoteSearchPageWindow::kMaxHotPages);
    }

    void cache_hit_applies_without_duplicate() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;

        QVERIFY(window
                    .applyPage(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2), true, true, 0)
                    .ok);
        window.putCache(2, makeRows('b', 2), true);

        const auto cached =
            window.tryApplyCached(&fx.model, 2, RemoteSearchPageWindow::FetchMode::Append, false, 0);
        QVERIFY(cached.ok);
        QCOMPARE(fx.model.count(), 4);
        QCOMPARE(window.slices.size(), 2);

        const auto again =
            window.tryApplyCached(&fx.model, 2, RemoteSearchPageWindow::FetchMode::Append, false, 1);
        QVERIFY(! again.ok);
        QCOMPARE(fx.model.count(), 4);
    }

    void clear_empties_slices_and_cache() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        QVERIFY(window
                    .applyPage(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 1), true, true, 0)
                    .ok);
        window.putCache(2, makeRows('b', 1), false);
        window.clear();
        QVERIFY(window.slicesEmpty());
        QVERIFY(! window.containsCache(2));
        QVERIFY(! window.hasPrevious());
    }

    void page_applied_blocks_reapply() {
        ModelFixture           fx;
        RemoteSearchPageWindow window;
        QVERIFY(window
                    .applyPage(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset,
                               makeRows('a', 2), true, true, 0)
                    .ok);
        QVERIFY(window.pageApplied(1));
        QVERIFY(! window.pageApplied(2));
        window.putCache(1, makeRows('x', 2), true);
        const auto dup =
            window.tryApplyCached(&fx.model, 1, RemoteSearchPageWindow::FetchMode::Reset, false, 0);
        QVERIFY(! dup.ok);
    }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    int              status = 0;
    {
        RemoteListModelTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        RemoteSearchPageWindowTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    return status;
}

#include "discover_tests.moc"
