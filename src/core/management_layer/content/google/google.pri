#
# Jenny Script fork: Google Docs / Google Drive integration
#
# Needs Qt >= 6.9 with the networkauth module. The OAuth client of the application is read from
# src/google_oauth.pri (not committed) or from the GOOGLE_OAUTH_CLIENT_ID and
# GOOGLE_OAUTH_CLIENT_SECRET environment variables (CI secrets).
#
GOOGLE_SRC_ROOT = $$clean_path($$PWD/../../../..)

versionAtLeast(QT_VERSION, 6.9.0):qtHaveModule(networkauth) {
    QT += networkauth
    DEFINES += GOOGLE_INTEGRATION

    exists($$GOOGLE_SRC_ROOT/google_oauth.pri): include($$GOOGLE_SRC_ROOT/google_oauth.pri)
    isEmpty(GOOGLE_OAUTH_CLIENT_ID): GOOGLE_OAUTH_CLIENT_ID = $$(GOOGLE_OAUTH_CLIENT_ID)
    isEmpty(GOOGLE_OAUTH_CLIENT_SECRET): GOOGLE_OAUTH_CLIENT_SECRET = $$(GOOGLE_OAUTH_CLIENT_SECRET)
    !isEmpty(GOOGLE_OAUTH_CLIENT_ID) {
        DEFINES += GOOGLE_OAUTH_CLIENT_ID=\\\"$$GOOGLE_OAUTH_CLIENT_ID\\\"
        DEFINES += GOOGLE_OAUTH_CLIENT_SECRET=\\\"$$GOOGLE_OAUTH_CLIENT_SECRET\\\"
    } else {
        !build_pass:message("Google OAuth client is not configured: Google sign in is disabled")
    }

    include($$GOOGLE_SRC_ROOT/3rd_party/qtkeychain/qtkeychain.pri)

    HEADERS += \
        $$PWD/google_auth_manager.h \
        $$PWD/google_drive_client.h

    SOURCES += \
        $$PWD/google_auth_manager.cpp \
        $$PWD/google_drive_client.cpp
} else {
    !build_pass:message("Qt >= 6.9 with networkauth is required for the Google integration")
}
