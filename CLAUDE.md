# CLAUDE.md

Fork của **Story Architect (STARC)**. Upstream là `story-apps/starc` (GPL-3.0, C++17, Qt 6, qmake).
Chủ fork: **Jenny Pixel Studio**. Đừng mặc định user rành C++/Qt: lỗi build và lệnh cần chạy phải giải thích rõ ràng.
Tên app tạm: **Jenny Script** (chốt ở Phase 0, quyết định D3).

**Kế hoạch: `docs/FORK_PLAN.md`.** Đọc file này trước khi nhận task mới. Cuối mỗi phiên, cập nhật mục *Trạng thái* ở cuối file.

## Cách làm việc
- Trả lời user bằng tiếng Việt, ngắn gọn, có bước làm cụ thể. Việc user phải tự làm (cài Qt, Google Cloud Console, test tay) thì hướng dẫn từng bước, lệnh copy-paste được.
- Code, comment, tên biến, commit message viết bằng **tiếng Anh**. Comment cũ của upstream (tiếng Nga) giữ nguyên, không dịch.
- Mỗi phase một branch (`phase-0-foundation`, `phase-1-gdocs`, …). Chỉ báo "xong" khi đủ ba điều: build được, test tự động pass, có checklist test tay cho user.
- **Hỏi user trước khi:** thêm dependency; sửa `.gitmodules` hoặc CI; đổi schema/định dạng file `.starc`; xin scope Google rộng hơn `drive.file`; push lên remote; mở PR/issue ở upstream.
- Code mới nằm trong file/thư mục mới. Chỉ chèn "điểm móc" tối thiểu vào file upstream, để sau này merge upstream ít conflict. Ghi danh sách file upstream đã sửa vào `README_FORK.md`.
- Môi trường không có GUI thì không tự test tay được. Khi đó dựa vào unit test, rồi đưa checklist cho user test trên máy thật.

## Sự thật về repo (kiểm ngày 03/11/2026, upstream v0.8.4)
- **Repo public không có** `src/cloud` (submodule private `story-apps/starcloud`) và khoảng 25 plugin private trong `src/core/management_layer/plugins/`: cards, timeline, mind map, breakdown, statistics, character/location/world information, collaborators, images gallery, presentation… Các thư mục này rỗng là bình thường, vì `plugins.pro` dùng `exists()` nên vẫn build được. Data model của nhân vật, bối cảnh, world… vẫn public trong `src/corelib/business_layer/model/`.
- **Cấm** chạy `git submodule update --init --recursive` không kèm đường dẫn (sẽ đòi quyền vào repo private). Chỉ init submodule public:
  `git submodule update --init --recursive src/3rd_party/pdfhummus src/3rd_party/pdftextextraction src/3rd_party/qtkeychain`
- Cloud gốc được nối vào qua `#ifdef CLOUD_SERVICE_MANAGER` trong `src/core/management_layer/application_manager.cpp`. Lớp `CloudServiceManager` có khoảng 95 hàm/signal. Không viết lại lớp này, trừ khi đang làm Phase 3.
- File project `.starc` là SQLite (`src/corelib/data_layer/database.cpp`, driver QSQLITE). **Không đổi schema.** Metadata đồng bộ Google lưu bên ngoài file (QSettings, khóa theo UUID).
- Import/export có sẵn trong `src/corelib/business_layer/import/` và `export/`. Screenplay hỗ trợ Fountain, FDX, DOCX, PDF; import thêm được Celtx, Trelby, KIT Scenarist. Manager phía UI: `src/core/management_layer/content/import/` và `content/export/`.
- Từ khóa nhận diện scene heading của Fountain importer nằm trong `sceneHeadingsDictionary()`, file `src/corelib/business_layer/import/abstract_fountain_importer.cpp` (chuỗi dịch được qua `QCoreApplication::translate`).
- Định danh app: `src/app/application.cpp` (`setApplicationName("Story Architect")`, `setOrganizationName("Story Apps")`, `setOrganizationDomain("storyapps.dev")`).
- Gửi crash lên BugSplat: trong `application_manager.cpp` (hàm `bugsplatDatabaseName()`, URL `*.bugsplat.com`). App còn gọi `starc.app/api/services/*` (unsplash, gproxy), `starc.app/downloads/fonts/`, và tải từ điển hunspell từ GitHub `story-apps/starc-language-dictionaries`.
- Chưa có bản dịch tiếng Việt: biến `TRANSLATIONS` trong `src/starc.pro` không có `vi`.
- Style: theo `src/.clang-format` (CI upstream dùng clang-format 20). Chỉ format phần mình sửa (`git clang-format`), không format lại cả file upstream.

## Build
Dùng Qt **6.11.x**, tối thiểu **6.8** vì cần PKCE của QtNetworkAuth. Modules: `qt5compat qtmultimedia qtspeech qtwebsockets qtnetworkauth` (thêm `qtimageformats` trên Windows). Fork chỉ build Qt 6, bỏ Qt 5.

Linux / macOS:
```bash
git submodule update --init --recursive src/3rd_party/pdfhummus src/3rd_party/pdftextextraction src/3rd_party/qtkeychain
cd build && ./build_crashpad.sh && cd ..     # tải depot_tools từ chromium.googlesource.com
cd src && qmake starc.pro CONFIG+=release && make -j8
./_build/starcapp                            # macOS: src/_build/starcapp.app
```

Windows: mở Developer Command Prompt x64 của VS 2022, có Qt `msvc2022_64` trong PATH. Cần OpenSSL 3 ở đường dẫn không có dấu cách (set `OPENSSL_PREFIX`), Git Bash và jom (hoặc nmake).
```bat
cd build && bash ./build_crashpad.sh -a x64 && cd ..
cd src && qmake starc.pro CONFIG+=release CRASHPAD_ARCH=x64 && jom -j4
```
Các bước chuẩn nhất nằm trong job chạy cho `pull_request` của `.github/workflows/build_starc_*.yml`. Đó là cách build không cần submodule private.

Cài Qt bằng CLI (Linux/CI): `pip install aqtinstall`, rồi
`aqt install-qt linux desktop 6.11.2 linux_gcc_64 -m qt5compat qtmultimedia qtspeech qtwebsockets qtnetworkauth`
(kiểm lại tên module bằng `aqt list-qt linux desktop --modules 6.11.2 linux_gcc_64`). Gói hệ thống Ubuntu cần cài: xem bước "Install dependencies" trong `build_starc_linux.yml`.

Test: dùng Qt Test, chạy headless với `QT_QPA_PLATFORM=offscreen`. Mẫu tham khảo: `tests/remote_text_changes/`.
Nếu môi trường chặn mạng tới chromium.googlesource.com thì không build được Crashpad. Khi đó dùng `CONFIG+=no_crashpad` (Phase 0, task 0.7) hoặc build qua GitHub Actions.

## Google (Phase 1–2): quy ước
- OAuth 2.0 cho desktop: `QOAuth2AuthorizationCodeFlow` + PKCE S256 + `QOAuthHttpServerReplyHandler` (redirect loopback 127.0.0.1, mở trình duyệt hệ thống). Trong qmake: `qtHaveModule(networkauth) { QT += networkauth; DEFINES += GOOGLE_INTEGRATION }`.
- Scope mặc định: `openid email https://www.googleapis.com/auth/drive.file`. **Không** dùng scope `documents` (thuộc loại sensitive). Tạo/cập nhật Google Doc bằng Drive API (upload kèm convert sang `application/vnd.google-apps.document`), kéo về bằng `files.export` dạng `text/plain`.
- Client ID/secret **không commit**. Đọc từ `src/google_oauth.pri` (thêm file này vào `.gitignore`), hoặc từ GitHub Secrets khi build trên CI.
- Refresh token lưu trong keychain của hệ điều hành (QtKeychain, hỏi user trước khi thêm). Không lưu dạng plain text.
- Trước khi code phần OAuth/Drive, đọc lại tài liệu Google hiện hành (scope, quota, verification) vì chính sách hay thay đổi.

## Không được làm
- Commit secret, token, file `.starc` hay kịch bản thật của user.
- Sửa code trong `src/3rd_party/` khi chưa hỏi.
- Gửi dữ liệu hoặc crash report của user về server upstream.
- Sửa file `LICENSE` hoặc xóa header bản quyền của upstream.
