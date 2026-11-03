#include <business_layer/import/fountain_text_normalizer.h>
#include <business_layer/import/screenplay/screenplay_fountain_importer.h>

#include <QXmlStreamReader>
#include <QtTest>

using BusinessLayer::FountainTextNormalizer;

namespace {

using Paragraph = QPair<QString, QString>;

/**
 * @brief Extract (type, text) of screenplay paragraphs from the importer XML
 */
QVector<Paragraph> paragraphs(const QString& _xml)
{
    static const QSet<QString> kTypes = {
        "scene_heading", "action", "character", "parenthetical", "dialogue", "transition",
    };
    QVector<Paragraph> result;
    QString currentType;
    QXmlStreamReader reader(_xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        const auto name = reader.name().toString();
        if (kTypes.contains(name)) {
            currentType = name;
        } else if (name == "v" && !currentType.isEmpty()) {
            result.append({ currentType, reader.readElementText().trimmed() });
            currentType.clear();
        }
    }
    return result;
}

QVector<Paragraph> import(const QString& _fountain)
{
    BusinessLayer::ScreenplayFountainImporter importer;
    return paragraphs(importer.screenplayText("\n" + _fountain).text);
}

QStringList textsOf(const QVector<Paragraph>& _paragraphs, const QString& _type)
{
    QStringList texts;
    for (const auto& paragraph : _paragraphs) {
        if (paragraph.first == _type) {
            texts.append(paragraph.second);
        }
    }
    return texts;
}

} // namespace


class FountainImportTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizeLine_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        auto row = [](const char* _name, const char* _input, const char* _expected) {
            QTest::newRow(_name) << QString::fromUtf8(_input) << QString::fromUtf8(_expected);
        };
        row("number-left", "1 EXT. ĐƯỜNG ĐÈO - ĐÊM", "EXT. ĐƯỜNG ĐÈO - ĐÊM");
        row("number-tab-both-sides", "12A\tINT. XE - ĐANG CHẠY - ĐÊM\t12A",
            "INT. XE - ĐANG CHẠY - ĐÊM");
        row("number-wide-gap-right", "7 EXT. SÂN - NGÀY      7", "EXT. SÂN - NGÀY");
        row("day-number-is-kept", "3 EXT. TRẠI - NGÀY 3", "EXT. TRẠI - NGÀY 3");
        row("int-ext", "4 INT./EXT. XE - ĐÊM", "INT./EXT. XE - ĐÊM");
        row("forced-heading", "5 .PHÒNG KHÁCH", ".PHÒNG KHÁCH");
        row("action-starting-with-number", "3 người bước vào.", "3 người bước vào.");
        row("vietnamese-heading", "NỘI. QUÁN CÀ PHÊ - NGÀY", ".NỘI. QUÁN CÀ PHÊ - NGÀY");
        row("vietnamese-heading-numbered", "2 NGOẠI. SÂN - CHIỀU", ".NGOẠI. SÂN - CHIỀU");
        row("vietnamese-int-ext", "NỘI/NGOẠI. XE - ĐÊM", ".NỘI/NGOẠI. XE - ĐÊM");
        row("word-starting-like-heading", "NỘI dung bức thư rất ngắn.",
            "NỘI dung bức thư rất ngắn.");
        row("ellipsis", "..xin lỗi con của ba.", "..xin lỗi con của ba.");
        row("non-breaking-space", "EXT.\xC2\xA0ĐƯỜNG - ĐÊM", "EXT. ĐƯỜNG - ĐÊM");
    }
    void normalizeLine()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(FountainTextNormalizer::normalize(input), expected);
    }

    void normalizeLineBreaks()
    {
        const QString input = QString::fromUtf8("\xEF\xBB\xBFTUẤN\r\nDòng một\x0B"
                                                "Dòng hai\r\n");
        QCOMPARE(FountainTextNormalizer::normalize(input),
                 QString::fromUtf8("TUẤN\nDòng một\nDòng hai\n"));
    }

    void importTextCopiedFromGoogleDocs()
    {
        const QString fountain = QStringLiteral(
            "1 EXT. ĐƯỜNG ĐÈO - ĐÊM\n"
            "\n"
            "Rừng thông đen kịt. Hai đốm đèn pha bò lên dốc.\n"
            "\n"
            "2 INT. XE TUẤN - ĐANG CHẠY - ĐÊM\n"
            "\n"
            "Tuấn lái xe, mắt đỏ hoe.\n"
            "\n"
            "TUẤN\n"
            "(thì thầm)\n"
            "Xin lỗi em...\n"
            "\n"
            "Anh chớp mắt.\n"
            "\n"
            "TUẤN\n"
            "..xin lỗi con.\n"
            "\n"
            "3 NỘI. NHÀ MAI - SÁNG\n"
            "\n"
            "Mai ngồi bên cửa sổ.\n"
            "\n"
            "..Ừ.\n");

        const auto result = import(fountain);

        QCOMPARE(textsOf(result, "scene_heading"),
                 QStringList({ "EXT. ĐƯỜNG ĐÈO - ĐÊM", "INT. XE TUẤN - ĐANG CHẠY - ĐÊM",
                               "NỘI. NHÀ MAI - SÁNG" }));
        QCOMPARE(textsOf(result, "character"), QStringList({ "TUẤN", "TUẤN" }));
        QCOMPARE(textsOf(result, "dialogue"), QStringList({ "Xin lỗi em...", "..xin lỗi con." }));
        QCOMPARE(textsOf(result, "parenthetical").size(), 1);
        QVERIFY(textsOf(result, "parenthetical").constFirst().contains("thì thầm"));
        QVERIFY(textsOf(result, "action").contains("Rừng thông đen kịt. Hai đốm đèn pha bò lên dốc."));
        QVERIFY(textsOf(result, "action").contains("..Ừ."));
    }
};

QTEST_MAIN(FountainImportTest)

#include "tst_fountain_import.moc"
