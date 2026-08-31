module;
#include "QExtra/macro_qt.hpp"

#ifdef Q_MOC_RUN
#    include "waywallen/query/log_query.moc"
#endif

export module waywallen:query.log;
export import :query.query;

namespace waywallen
{

export class DaemonLogQuery : public Query,
                              public QueryExtra<control::v1::Response, DaemonLogQuery> {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString path READ path NOTIFY pathChanged FINAL)
    Q_PROPERTY(QString content READ content NOTIFY contentChanged FINAL)
    Q_PROPERTY(bool truncated READ truncated NOTIFY truncatedChanged FINAL)

public:
    DaemonLogQuery(QObject* parent = nullptr);

    auto path() const -> const QString&;
    auto content() const -> const QString&;
    auto truncated() const -> bool;

    void reload() override;

    Q_SIGNAL void pathChanged();
    Q_SIGNAL void contentChanged();
    Q_SIGNAL void truncatedChanged();

private:
    QString m_path;
    QString m_content;
    bool    m_truncated = false;
};

} // namespace waywallen
