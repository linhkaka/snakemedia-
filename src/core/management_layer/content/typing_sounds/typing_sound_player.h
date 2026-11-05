#pragma once

#include <QObject>

class QKeyEvent;

namespace ManagementLayer {

/**
 * @brief Jenny Script fork: plays the sound pack chosen in the settings when the user types
 */
class TypingSoundPlayer : public QObject
{
    Q_OBJECT

public:
    explicit TypingSoundPlayer(QObject* _parent = nullptr);
    ~TypingSoundPlayer() override;

    void playForKey(const QKeyEvent* _event);

    /**
     * @brief Let the user hear the pack that was just chosen
     */
    void preview();

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
