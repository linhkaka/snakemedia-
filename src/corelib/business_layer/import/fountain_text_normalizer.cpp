#include "fountain_text_normalizer.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>


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

/**
 * @brief Stricter check for a line without the blank lines around it: "INTERNET" is not a heading
 */
bool isStandaloneSceneHeading(const QString& _line)
{
    if (_line.startsWith('.')) {
        return !_line.startsWith("..");
    }
    for (const auto& prefix : kSceneHeadings) {
        if (_line.startsWith(prefix) && _line.size() > prefix.size()
            && (_line.at(prefix.size()) == '.' || _line.at(prefix.size()) == ' ')) {
            return true;
        }
    }
    return false;
}

bool isParenthetical(const QString& _line)
{
    return _line.startsWith('(') && _line.endsWith(')');
}

/**
 * @brief Uppercase short line without closing punctuation: "QUANG", "QUANG (V.O.)"
 */
bool isCharacterCue(const QString& _line)
{
    if (_line.startsWith('@')) {
        return true;
    }

    static const QRegularExpression kExtension(QStringLiteral(R"(\s*\([^)]*\)\s*\^?$)"));
    const QString name = QString(_line).remove(kExtension).simplified();
    constexpr int kMaximumLength = 40;
    constexpr int kMaximumWords = 5;
    if (name.isEmpty() || name.size() > kMaximumLength || name.count(' ') >= kMaximumWords
        || name != name.toUpper()) {
        return false;
    }
    if (!std::any_of(name.cbegin(), name.cend(), [](QChar _char) { return _char.isLetter(); })) {
        return false;
    }

    static const QString kClosingPunctuation = QString::fromUtf8(".!?,:;…\"'”");
    return !kClosingPunctuation.contains(name.back());
}

/**
 * @brief Proper Fountain has a blank line every few lines, even when it is hard wrapped
 */
bool hasHardlyAnyBlankLines(const QStringList& _lines)
{
    int filledLines = 0;
    int blankLineRuns = 0;
    bool isPreviousBlank = false;
    for (const auto& line : _lines) {
        if (line.trimmed().isEmpty()) {
            if (filledLines > 0 && !isPreviousBlank) {
                ++blankLineRuns;
            }
            isPreviousBlank = true;
        } else {
            ++filledLines;
            isPreviousBlank = false;
        }
    }
    if (isPreviousBlank && blankLineRuns > 0) {
        --blankLineRuns;
    }
    return filledLines >= 2 && blankLineRuns * 10 < filledLines;
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

QString FountainTextNormalizer::formatPlainText(const QString& _text)
{
    const QString normalized = normalize(_text);
    const auto lines = normalized.split('\n');
    if (!hasHardlyAnyBlankLines(lines)) {
        return normalized;
    }

    QStringList rows;
    for (const auto& line : lines) {
        if (const auto row = line.trimmed(); !row.isEmpty()) {
            rows.append(row);
        }
    }

    QStringList result;
    const auto addBlock = [&result](const QStringList& _block) {
        if (!result.isEmpty()) {
            result.append(QString());
        }
        result.append(_block);
    };
    const auto isDialogue = [](const QString& _row) {
        return !isStandaloneSceneHeading(_row) && !isCharacterCue(_row);
    };

    for (int index = 0; index < rows.size(); ++index) {
        const auto& row = rows.at(index);
        const bool isCue = !isStandaloneSceneHeading(row) && isCharacterCue(row)
            && index + 1 < rows.size() && isDialogue(rows.at(index + 1));
        if (!isCue) {
            addBlock({ row });
            continue;
        }

        //
        // Cue, then any number of "(parenthetical) + dialogue line" pairs
        //
        QStringList block = { row };
        int next = index + 1;
        forever
        {
            while (next < rows.size() && isParenthetical(rows.at(next))) {
                block.append(rows.at(next++));
            }
            if (next < rows.size() && isDialogue(rows.at(next))) {
                block.append(rows.at(next++));
            }
            if (next >= rows.size() || !isParenthetical(rows.at(next))) {
                break;
            }
        }
        addBlock(block);
        index = next - 1;
    }
    return result.join('\n');
}

} // namespace BusinessLayer
