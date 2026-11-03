#include "google_account_card.h"

#include "google_auth_manager.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/label/label.h>

#include <QGridLayout>
#include <QHBoxLayout>


namespace ManagementLayer {

class GoogleAccountCard::Implementation
{
public:
    Implementation(GoogleAccountCard* _q, GoogleAuthManager* _authManager);

    void updateState();

    GoogleAccountCard* q = nullptr;
    GoogleAuthManager* authManager = nullptr;
    QGridLayout* layout = nullptr;
    H5Label* title = nullptr;
    Body1Label* status = nullptr;
    Button* signInButton = nullptr;
    Button* signOutButton = nullptr;
};

GoogleAccountCard::Implementation::Implementation(GoogleAccountCard* _q,
                                                  GoogleAuthManager* _authManager)
    : q(_q)
    , authManager(_authManager)
    , layout(new QGridLayout)
    , title(new H5Label(_q))
    , status(new Body1Label(_q))
    , signInButton(new Button(_q))
    , signOutButton(new Button(_q))
{
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(title, 0, 0);
    auto buttonsLayout = new QHBoxLayout;
    buttonsLayout->setContentsMargins({});
    buttonsLayout->setSpacing(0);
    buttonsLayout->addWidget(status, 0, Qt::AlignVCenter);
    buttonsLayout->addWidget(signInButton);
    buttonsLayout->addWidget(signOutButton);
    buttonsLayout->addStretch();
    layout->addLayout(buttonsLayout, 1, 0);
    q->setContentLayout(layout);
}

void GoogleAccountCard::Implementation::updateState()
{
    const bool isSignedIn = authManager->isSignedIn();
    if (!GoogleAuthManager::isConfigured()) {
        status->setText(GoogleAccountCard::tr("Google sign in is not available in this build"));
    } else if (isSignedIn) {
        const auto email = authManager->email();
        status->setText(email.isEmpty() ? GoogleAccountCard::tr("Signed in") : GoogleAccountCard::tr("Signed in as %1").arg(email));
    } else {
        status->setText(GoogleAccountCard::tr("Not signed in. Sign in to push screenplays to Google Docs"));
    }
    signInButton->setText(isSignedIn ? GoogleAccountCard::tr("Switch account") : GoogleAccountCard::tr("Sign in with Google"));
    signInButton->setVisible(GoogleAuthManager::isConfigured());
    signOutButton->setText(GoogleAccountCard::tr("Sign out"));
    signOutButton->setVisible(isSignedIn);
}


// ****


GoogleAccountCard::GoogleAccountCard(GoogleAuthManager* _authManager, QWidget* _parent)
    : Card(_parent)
    , d(new Implementation(this, _authManager))
{
    connect(d->signInButton, &Button::clicked, this, [this] {
        //
        // Switching account starts with a clean session
        //
        if (d->authManager->isSignedIn()) {
            d->authManager->signOut();
        }
        d->authManager->signIn();
    });
    connect(d->signOutButton, &Button::clicked, d->authManager, &GoogleAuthManager::signOut);
    connect(d->authManager, &GoogleAuthManager::signedInChanged, this, [this] {
        d->updateState();
    });

    updateTranslations();
    designSystemChangeEvent(nullptr);
}

GoogleAccountCard::~GoogleAccountCard() = default;

void GoogleAccountCard::updateTranslations()
{
    d->title->setText(tr("Google account"));
    d->updateState();
}

void GoogleAccountCard::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Card::designSystemChangeEvent(_event);

    setBackgroundColor(Ui::DesignSystem::color().background());

    auto titleColor = Ui::DesignSystem::color().onBackground();
    titleColor.setAlphaF(Ui::DesignSystem::inactiveTextOpacity());
    auto titleMargins = Ui::DesignSystem::label().margins().toMargins();
    titleMargins.setBottom(Ui::DesignSystem::compactLayout().px12());
    d->title->setBackgroundColor(Ui::DesignSystem::color().background());
    d->title->setTextColor(titleColor);
    d->title->setContentsMargins(titleMargins);

    auto labelMargins = Ui::DesignSystem::label().margins().toMargins();
    labelMargins.setTop(static_cast<int>(Ui::DesignSystem::button().shadowMargins().top()));
    labelMargins.setBottom(static_cast<int>(Ui::DesignSystem::button().shadowMargins().bottom()));
    d->status->setBackgroundColor(Ui::DesignSystem::color().background());
    d->status->setTextColor(Ui::DesignSystem::color().onBackground());
    d->status->setContentsMargins(labelMargins);

    for (auto button : { d->signInButton, d->signOutButton }) {
        button->setBackgroundColor(Ui::DesignSystem::color().accent());
        button->setTextColor(Ui::DesignSystem::color().accent());
    }

    d->layout->setContentsMargins(0, 0, 0, Ui::DesignSystem::layout().px8());
}

} // namespace ManagementLayer
