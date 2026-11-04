#pragma once

#include <QObject>
#include <QScopedPointer>

class QWidget;

namespace ManagementLayer {

/**
 * @brief Updates the portable Windows package from the GitHub releases of the fork
 *
 * Asks GitHub for the latest release when the app starts and stays silent when there is no
 * network. Nothing is installed without asking: the user confirms, the package is downloaded and
 * its SHA-256 is checked, then a script replaces the files after the app has closed and starts it
 * again. Projects are not touched, they are stored outside of the application folder.
 */
class AppUpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit AppUpdateManager(QWidget* _parentWidget, QObject* _parent = nullptr);
    ~AppUpdateManager() override;

    /**
     * @brief Ask GitHub for the latest release and offer it, if it is newer than this app
     */
    void checkForUpdate();

    /**
     * @brief Start the script that installs the downloaded package
     * @return True if the script is running and the app has to close now
     */
    bool startInstaller();

signals:
    /**
     * @brief The package is downloaded and checked. Save the work, then call startInstaller()
     */
    void installReady();

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
