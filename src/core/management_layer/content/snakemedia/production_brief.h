#pragma once

#include <QJsonObject>
#include <QString>

namespace SnakeMedia {

// A separate production manifest; the STARC database schema remains unchanged.
struct ProductionBrief {
    QString id;
    QString title;
    QString channel;
    QString slot;
    QString market = QStringLiteral("VN");
    QString genre = QStringLiteral("drama");
    QString status = QStringLiteral("draft");
    QString logline;
    QString script;
    QString characters;
    QString locations;
    QString notes;

    static ProductionBrief create();
    static bool fromJson(const QJsonObject& object, ProductionBrief& brief, QString& error);
    QJsonObject toJson() const;
    QJsonObject toStudioProject() const;
    QString handoffText() const;
    bool validate(QString& error) const;
    bool save(const QString& path, QString& error) const;
    static bool load(const QString& path, ProductionBrief& brief, QString& error);
};

} // namespace SnakeMedia
