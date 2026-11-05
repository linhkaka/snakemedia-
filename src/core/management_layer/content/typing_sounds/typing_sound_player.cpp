#include "typing_sound_player.h"

#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>

#include <QKeyEvent>
#include <QRandomGenerator>
#include <QSoundEffect>
#include <QUrl>
#include <QVector>


namespace ManagementLayer {

namespace {

const QString kTypewriterPack = QStringLiteral("typewriter");

/**
 * @brief The packs and the number of their key sounds, build/sounds/generate_typing_sounds.py makes
 *        the files of all of them but the typewriter, which is the original sound of the app
 */
int keySoundsCount(const QString& _pack)
{
    if (_pack == QLatin1String("mechanical")) {
        return 6;
    }
    if (_pack == QLatin1String("funny") || _pack == QLatin1String("piano")
        || _pack == QLatin1String("violin")) {
        return 8;
    }
    if (_pack == QLatin1String("drums")) {
        return 7;
    }
    return 0;
}

} // namespace


class TypingSoundPlayer::Implementation
{
public:
    explicit Implementation(TypingSoundPlayer* _q);

    QSoundEffect* makeSound(const QString& _folder, const QString& _name) const;
    void loadCurrentPack();
    void playRandomKey();

    TypingSoundPlayer* q = nullptr;
    QString pack;
    QSoundEffect* returnSound = nullptr;
    QSoundEffect* spaceSound = nullptr;
    QSoundEffect* deleteSound = nullptr;
    QVector<QSoundEffect*> keySounds;
    int lastKeySound = -1;
};

TypingSoundPlayer::Implementation::Implementation(TypingSoundPlayer* _q)
    : q(_q)
{
}

QSoundEffect* TypingSoundPlayer::Implementation::makeSound(const QString& _folder,
                                                           const QString& _name) const
{
    auto sound = new QSoundEffect(q);
    sound->setSource(QUrl::fromLocalFile(_folder + _name));
    return sound;
}

void TypingSoundPlayer::Implementation::loadCurrentPack()
{
    auto currentPack = settingsValue(DataStorageLayer::kApplicationTypingSoundPackKey).toString();
    const int count = keySoundsCount(currentPack);
    if (count == 0) {
        currentPack = kTypewriterPack;
    }
    if (currentPack == pack) {
        return;
    }
    pack = currentPack;

    for (auto sound : std::as_const(keySounds)) {
        sound->deleteLater();
    }
    keySounds.clear();
    lastKeySound = -1;
    if (returnSound != nullptr) {
        returnSound->deleteLater();
        spaceSound->deleteLater();
        deleteSound->deleteLater();
    }

    const bool isTypewriter = pack == kTypewriterPack;
    const QString folder
        = isTypewriter ? QStringLiteral(":/audio/") : QStringLiteral(":/typing-sounds/%1/").arg(pack);
    returnSound = makeSound(folder, QStringLiteral("return"));
    spaceSound = makeSound(folder, QStringLiteral("space"));
    deleteSound = makeSound(folder, QStringLiteral("backspace"));
    const int keysCount = isTypewriter ? 4 : count;
    for (int key = 1; key <= keysCount; ++key) {
        keySounds.append(makeSound(folder, QStringLiteral("key-%1").arg(key, 2, 10, QLatin1Char('0'))));
    }
}

void TypingSoundPlayer::Implementation::playRandomKey()
{
    //
    // Never the same sound twice in a row: the notes of the music packs make a melody this way
    //
    int key = QRandomGenerator::global()->bounded(keySounds.size());
    if (key == lastKeySound) {
        key = (key + 1) % keySounds.size();
    }
    lastKeySound = key;
    keySounds.at(key)->play();
}


// ****


TypingSoundPlayer::TypingSoundPlayer(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(this))
{
}

TypingSoundPlayer::~TypingSoundPlayer() = default;

void TypingSoundPlayer::playForKey(const QKeyEvent* _event)
{
    d->loadCurrentPack();

    switch (_event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        d->returnSound->play();
        break;
    }

    case Qt::Key_Space: {
        d->spaceSound->play();
        break;
    }

    case Qt::Key_Backspace:
    case Qt::Key_Delete: {
        d->deleteSound->play();
        break;
    }

    default: {
        if (!_event->text().isEmpty()) {
            d->playRandomKey();
        }
        break;
    }
    }
}

void TypingSoundPlayer::preview()
{
    d->loadCurrentPack();
    d->playRandomKey();
}

} // namespace ManagementLayer
