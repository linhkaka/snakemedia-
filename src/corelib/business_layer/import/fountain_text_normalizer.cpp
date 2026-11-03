#include "fountain_text_normalizer.h"

#include <QRegularExpression>
#include <QStringList>


namespace BusinessLayer {

namespace {

/**
 * @brief Scene heading prefixes recognized by the Fountain importer
 */
const QStringList kSceneHeadings = {
    "INT./EXT", "INT/EXT", "EXT./INT", "EXT/INT", "I/E", "INT", "EXT", "EST",
};

/**
 * @brief Vietnamese scene heading prefixes, they are forced with the "." Fountain prefix
 */
const QStringList kVietnameseSceneHeadings = {
    QString::fromUtf8("NỘI/NGOẠI."),
    QString::fromUtf8("NGOẠI/NỘI."),
    QString::fromUtf8("NỘI."),
    QString::fromUtf8("NGOẠI."),
};

bool startsWithAny(const QString& _text, const QStringList& _prefixes)
{
    for (const auto& prefix : _prefixes) {
        if (_text.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

bool isSceneHeading(const QString& _text)
{
    return startsWithAny(_text, kSceneHeadings) || startsWithAny(_text, kVietnameseSceneHeadings)
        || (_text.startsWith('.') && !_text.startsWith(".."));
}

} // namespace


QString FountainTextNormalizer::normalize(const QString& _text)
{
    QString text = _text;
    text.remove(QChar(0xFEFF));
    text.remove('\r');
    //
    // Soft line breaks (Google Docs exports Shift+Enter as a vertical tab)
    //
    text.replace(QChar(0x000B), '\n');
    text.replace(QChar(0x2028), '\n');
    text.replace(QChar(0x2029), '\n');
    text.replace(QChar(0x00A0), ' ');

    //
    // "12 EXT. ROAD - NIGHT" or "12A\tEXT. ROAD - NIGHT\t12A"
    //
    static const QRegularExpression kLeadingSceneNumber(
        QStringLiteral(R"(^[\t ]*(\d{1,4}[A-Za-z]{0,2})[\t ]+(\S.*)$)"));

    auto lines = text.split('\n');
    for (auto& line : lines) {
        const auto match = kLeadingSceneNumber.match(line);
        if (match.hasMatch()) {
            const QString number = match.captured(1);
            QString heading = match.captured(2);
            if (isSceneHeading(heading)) {
                //
                // Scene number on the right side is separated by a tab or a wide gap, a single
                // space is kept to not break headings like "EXT. CAMP - DAY 1"
                //
                const QRegularExpression trailingNumber(
                    QStringLiteral("(\\t|  )[\\t ]*") + QRegularExpression::escape(number)
                    + QStringLiteral("[\\t ]*$"));
                heading.remove(trailingNumber);
                line = heading.trimmed();
            }
        }

        const QString trimmed = line.trimmed();
        if (startsWithAny(trimmed, kVietnameseSceneHeadings)) {
            line = '.' + trimmed;
        }
    }
    return lines.join('\n');
}

} // namespace BusinessLayer
