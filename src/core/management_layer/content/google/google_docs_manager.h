#pragma once

#include <QObject>

class QUuid;
class QWidget;

namespace ManagementLayer {

class GoogleAuthManager;
class ProjectManager;

/**
 * @brief Google Docs bridge for screenplays
 *
 * Push: the screenplay is exported as Fountain text and uploaded as a Google Doc into the
 * "Jenny Script/<project>" folder, next pushes update the same Doc. Before an update the Doc text
 * is compared with the text of the last sync to not overwrite edits made in Google Docs.
 * Pull: the Doc is exported as plain text, imported as Fountain and added as a new draft.
 *
 * Mapping between a document and its Google Doc is kept in the application settings, the project
 * file is not changed.
 */
class GoogleDocsManager : public QObject
{
    Q_OBJECT

public:
    enum Action {
        Push = 0,
        Pull = 1,
        OpenInBrowser = 2,
    };

public:
    GoogleDocsManager(QObject* _parent, QWidget* _parentWidget, ProjectManager* _projectManager);
    ~GoogleDocsManager() override;

    GoogleAuthManager* authManager() const;

    void handleAction(int _action, const QUuid& _documentUuid);

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
