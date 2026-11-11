#include <production_brief.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

class ProductionBriefTest : public QObject
{
    Q_OBJECT
private slots:
    void vietnameseRoundTrip()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        auto original = SnakeMedia::ProductionBrief::create();
        original.title = QStringLiteral("Bữa cơm cuối cùng");
        original.channel = QStringLiteral("Chuyện gia đình");
        original.characters = QStringLiteral("Lan — con dâu, luôn xưng con.");
        original.script = QStringLiteral("NGOẠI. VƯỜN - ĐÊM\nLan trở về.\n\n...Con xin lỗi.");
        QString error;
        const auto path = folder.filePath("episode.snake.json");
        QVERIFY2(original.save(path, error), qPrintable(error));
        SnakeMedia::ProductionBrief restored;
        QVERIFY2(SnakeMedia::ProductionBrief::load(path, restored, error), qPrintable(error));
        QCOMPARE(restored.toJson(), original.toJson());
        QVERIFY(restored.handoffText().contains(original.characters));
    }

    void invalidInputDoesNotReplaceCurrentBrief()
    {
        auto brief = SnakeMedia::ProductionBrief::create();
        brief.title = "Keep me";
        const auto before = brief.toJson();
        QString error;
        QVERIFY(!SnakeMedia::ProductionBrief::fromJson(QJsonObject{ { "schema", 2 } }, brief, error));
        QCOMPARE(brief.toJson(), before);
        auto bad = before;
        bad.insert("script", QJsonArray());
        QVERIFY(!SnakeMedia::ProductionBrief::fromJson(bad, brief, error));
        QCOMPARE(brief.toJson(), before);
    }

    void failedSaveKeepsPreviousFile()
    {
        QTemporaryDir folder;
        auto brief = SnakeMedia::ProductionBrief::create();
        brief.title = "Keep me";
        QString error;
        const auto path = folder.filePath("episode.snake.json");
        QVERIFY(brief.save(path, error));
        brief.title.clear();
        QVERIFY(!brief.save(path, error));
        SnakeMedia::ProductionBrief restored;
        QVERIFY(SnakeMedia::ProductionBrief::load(path, restored, error));
        QCOMPARE(restored.title, QString("Keep me"));
    }

    void studioHandoffHasNoSecretsOrExternalPaths()
    {
        auto brief = SnakeMedia::ProductionBrief::create();
        brief.title = QStringLiteral("Tập thử");
        brief.characters = QStringLiteral("Lan — nhân vật hư cấu");
        brief.script = "Hello";
        const auto project = brief.toStudioProject();
        QCOMPARE(project.value("market").toString(), QString("VN"));
        QCOMPARE(project.value("scenes").toArray().first().toObject().value("text").toString(), brief.script);
        QCOMPARE(project.value("notes").toArray().size(), 3);
        QVERIFY(!project.contains("api_key"));
        QVERIFY(!project.contains("path"));
        QVERIFY(!project.contains("schema"));
    }

    void oversizedAndMissingFilesAreRejected()
    {
        auto brief = SnakeMedia::ProductionBrief::create();
        brief.title = "Episode";
        brief.script.fill('a', 2 * 1024 * 1024);
        QString error;
        QVERIFY(!brief.validate(error));
        QTemporaryDir folder;
        QVERIFY(!SnakeMedia::ProductionBrief::load(folder.filePath("missing.snake.json"), brief, error));
    }
};

QTEST_APPLESS_MAIN(ProductionBriefTest)
#include "tst_production_brief.moc"
