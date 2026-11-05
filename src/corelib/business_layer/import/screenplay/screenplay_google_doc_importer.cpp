#include "screenplay_google_doc_importer.h"

#include <business_layer/model/text/text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/text_template.h>
#include <utils/helpers/text_helper.h>

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>

#include <algorithm>


namespace BusinessLayer {

namespace {

/**
 * @brief Character, parenthetical and dialogue are at least 1 inch from the action (96 px), a
 *        Google Docs user can't move a paragraph this far by accident
 */
constexpr int kMinimumIndent = 40;

/**
 * @brief The same text in the app and in the Doc: parentheses of a parenthetical are added on
 *        export, and some paragraphs are uppercase only because of the template
 */
QString paragraphKey(const QString& _text)
{
    QString key = _text.simplified();
    key.replace(QChar(0x2013), '-');
    if (key.startsWith('(') && key.endsWith(')')) {
        key = key.mid(1, key.size() - 2).simplified();
    }
    return TextHelper::smartToUpper(key);
}

bool hasLetters(const QString& _text)
{
    return std::any_of(_text.cbegin(), _text.cend(), [](QChar _char) { return _char.isLetter(); });
}

bool isUppercase(const QString& _text)
{
    return hasLetters(_text) && _text == TextHelper::smartToUpper(_text);
}

bool isSceneHeading(const QString& _text)
{
    static const QRegularExpression kPrefix(QString::fromUtf8(
        "^(\\.|\\d{1,4}[A-Z]?[. ]|(INT|EXT|EST|I/E|NỘI|NGOẠI|CẢNH|SCENE)([./ :]|$))"));
    return kPrefix.match(_text).hasMatch() || _text.contains(QLatin1String(" - "));
}

void collectParagraphs(const TextModelItem* _item, ScreenplayGoogleDocImporter& _importer)
{
    for (int row = 0; row < _item->childCount(); ++row) {
        const auto child = static_cast<const TextModelItem*>(_item->childAt(row));
        if (child->type() == TextModelItemType::Text) {
            const auto textItem = static_cast<const TextModelTextItem*>(child);
            _importer.addKnownParagraph(textItem->text(), textItem->paragraphType());
        } else {
            collectParagraphs(child, _importer);
        }
    }
}

} // namespace


void ScreenplayGoogleDocImporter::addKnownParagraphs(const TextModel* _model)
{
    if (_model != nullptr) {
        collectParagraphs(_model->itemForIndex({}), *this);
    }
}

void ScreenplayGoogleDocImporter::addKnownParagraph(const QString& _text, TextParagraphType _type)
{
    //
    // Headers of folders would need groups in the imported xml, they are detected as action
    //
    switch (_type) {
    case TextParagraphType::SceneHeading:
    case TextParagraphType::SceneCharacters:
    case TextParagraphType::Action:
    case TextParagraphType::Character:
    case TextParagraphType::Parenthetical:
    case TextParagraphType::Dialogue:
    case TextParagraphType::Lyrics:
    case TextParagraphType::Transition:
    case TextParagraphType::Shot: {
        break;
    }
    default: {
        return;
    }
    }

    const auto key = paragraphKey(_text);
    if (key.isEmpty()) {
        return;
    }
    const auto known = m_knownParagraphs.constFind(key);
    m_knownParagraphs.insert(key,
                             known == m_knownParagraphs.constEnd() || known.value() == _type
                                 ? _type
                                 : TextParagraphType::Undefined);
}

TextParagraphType ScreenplayGoogleDocImporter::typeForTextCursor(const QTextCursor& _cursor,
                                                                 TextParagraphType _lastBlockType,
                                                                 int _prevEmptyLines,
                                                                 int _minLeftMargin) const
{
    const auto text = _cursor.block().text().simplified();

    const auto known = m_knownParagraphs.value(paragraphKey(text), TextParagraphType::Undefined);
    if (known != TextParagraphType::Undefined) {
        return known;
    }

    const auto blockFormat = _cursor.blockFormat();
    if (blockFormat.alignment().testFlag(Qt::AlignRight)) {
        return TextParagraphType::Transition;
    }

    if (blockFormat.leftMargin() - _minLeftMargin >= kMinimumIndent) {
        if (text.startsWith('(')) {
            return TextParagraphType::Parenthetical;
        }
        //
        // Character can have an extension: "CÓC MẸ (V.O.)"
        //
        static const QRegularExpression kExtension(QStringLiteral(R"(\([^)]*\))"));
        const bool hasSpaceBefore = _prevEmptyLines > 0 || blockFormat.topMargin() > 0
            || _cursor.block().previous().blockFormat().bottomMargin() > 0;
        if (isUppercase(QString(text).remove(kExtension).simplified()) && hasSpaceBefore
            && _lastBlockType != TextParagraphType::Character) {
            return TextParagraphType::Character;
        }
        return TextParagraphType::Dialogue;
    }

    if (isUppercase(text)) {
        if (isSceneHeading(text)) {
            return TextParagraphType::SceneHeading;
        }
        if (text.endsWith(':')) {
            return TextParagraphType::Transition;
        }
    }
    return TextParagraphType::Action;
}

void ScreenplayGoogleDocImporter::writeReviewMarks(QXmlStreamWriter& _writer,
                                                   QTextCursor& _cursor) const
{
    Q_UNUSED(_writer)
    Q_UNUSED(_cursor)
}

} // namespace BusinessLayer
