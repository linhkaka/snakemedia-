# Jenny Script

Jenny Script is a fork of [Story Architect (STARC)](https://github.com/story-apps/starc) maintained by
Jenny Pixel Studio. It adds a Google Docs / Google Drive bridge on top of the open-source part of STARC.

Based on Story Architect by Story Apps. Licensed under GPL-3.0, see [LICENSE](LICENSE).

Roadmap: [docs/FORK_PLAN.md](docs/FORK_PLAN.md).

## Differences from upstream

- Built from public sources only: the private `src/cloud` submodule and the private PRO plugins are not available.
- Crashpad / BugSplat crash reporting can be disabled with `CONFIG+=no_crashpad` (the fork CI always does this).
- Vietnamese UI translation: `src/core/translations/translation_vi.ts`. The `.qm` is not committed; CI builds it with
  `lrelease src/core/translations/translation_vi.ts -qm src/core/translations/translation_vi.qm`
  (run the same command before `qmake` when building locally).
- Own CI workflow: [.github/workflows/build_fork.yml](.github/workflows/build_fork.yml).
  Upstream workflows need private submodules and secrets; disable them in the GitHub UI
  (Actions → workflow → Disable workflow) instead of deleting them.

## Build

Qt 6.11.x (6.8 minimum), modules `qt5compat qtimageformats qtmultimedia qtspeech qtwebsockets qtnetworkauth`.

Init **only** the public submodules:

```bash
git submodule update --init --recursive src/3rd_party/pdfhummus src/3rd_party/pdftextextraction src/3rd_party/qtkeychain
```

Linux / macOS:

```bash
cd src && qmake starc.pro CONFIG+=release CONFIG+=no_crashpad && make -j8
```

Windows (x64 Native Tools Command Prompt for VS 2022, Qt `msvc2022_64` in `PATH`,
OpenSSL 3 at a path without spaces in `OPENSSL_PREFIX`):

```bat
cd src && qmake starc.pro CONFIG+=release CONFIG+=no_crashpad && jom -j4
```

## Syncing with upstream

Remotes: `origin` = this fork, `upstream` = `https://github.com/story-apps/starc`.
Once a month or after an upstream release:

```bash
git fetch upstream && git checkout master && git merge upstream/master
```

## Upstream files modified by the fork

Keep this list up to date: these are the files to check when merging upstream.

| File | Change |
|---|---|
| `src/core/core.pro` | Crashpad block wrapped in `!no_crashpad` |
| `src/core/management_layer/application_manager.cpp` | Crashpad includes, client, `initCrashpad()` and `sendCrashInfo()` guarded by `NO_CRASHPAD`; startup telemetry (`telemetry.storyapps.dev`) disabled in `sendStartupStatistics()` |
| `src/core/management_layer/application_manager.cpp` | `QLocale::Vietnamese` → `translation_vi.qm` |
| `src/core/ui/settings/language_dialog.cpp`, `src/core/ui/settings/settings_view.cpp` | "Tiếng Việt" in the language list |
| `src/core/ui/onboarding/onboarding_navigator.cpp`, `src/core/ui/onboarding/onboarding_view.cpp` | "Tiếng Việt" in the first-run language list |
| `src/starc.pro`, `src/core/translations.qrc` | `translation_vi.ts` / `translation_vi.qm` registered |
| `src/corelib/business_layer/import/abstract_fountain_importer.cpp` | input goes through `FountainTextNormalizer` (scene numbers, Vietnamese headings, Google Docs characters); a leading ellipsis (`..`) is no longer a forced scene heading |
| `src/corelib/corelib.pro` | `fountain_text_normalizer.{h,cpp}` registered |
| `src/corelib/business_layer/model/structure/structure_proxy_model.cpp` | statistics documents hidden in the navigator (their plugins are private) |
| `application_manager.cpp`, `onboarding_navigator.cpp`, `onboarding_view.cpp`, `language_dialog.cpp` | Vietnamese is the default and first language; onboarding saves the chosen language instead of the system one (upstream bug) |
| `src/corelib/utils/helpers/color_helper.cpp`, `src/corelib/ui/modules/comments/comments_toolbar.cpp` | industry standard revision colors (Blue, Pink, Yellow, Green, Goldenrod, Buff, Salmon, Cherry, Tan); upstream colors still map to the same level |
| `src/core/core.pro` | `include(management_layer/content/google/google.pri)` (Google integration, QtKeychain) |
