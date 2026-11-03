#pragma once

#include <ui/widgets/card/card.h>

namespace ManagementLayer {

class GoogleAuthManager;

/**
 * @brief Settings card with the Google account of the application: email, sign in and sign out
 */
class GoogleAccountCard : public Card
{
    Q_OBJECT

public:
    GoogleAccountCard(GoogleAuthManager* _authManager, QWidget* _parent = nullptr);
    ~GoogleAccountCard() override;

protected:
    void updateTranslations() override;
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
