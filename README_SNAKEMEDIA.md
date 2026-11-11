# SNAKEMEDIA Studio — bản desktop Windows

Nhánh phát triển: `phase-snakemedia-desktop`.
Nền: [Jenny Script / vienglacial/starc](https://github.com/vienglacial/starc), commit
`c07a6ccc37d9560e0c5493393e2a253a0c97c441`, dựa trên Story Architect của Story Apps.
GPL-3.0; giữ nguyên `LICENSE`, ghi công trong `NOTICE.txt`.

## Phần SNAKEMEDIA đã triển khai trong nguồn

- Tên app, organization/settings, biểu tượng cửa sổ/EXE và trang giới thiệu riêng.
- Projects/backups mặc định nằm trong `Documents/SNAKEMEDIA`, không dùng thư mục mặc định STARC.
- Tên keychain Google và thư mục Drive riêng; bản build mới cần OAuth client của SNAKEMEDIA.
- Giữ giao diện tiếng Việt và khả năng soạn thảo/nhập/xuất công khai của fork.
- Menu **Bàn sản xuất SNAKEMEDIA**: tên tập, kênh, slot, thị trường, thể loại, trạng thái,
  logline, bản thảo, khóa nhân vật/bối cảnh, nguồn và ghi chú duyệt.
- Hồ sơ `*.snake.json` ghi nguyên tử bằng `QSaveFile`, hỏi lưu thay đổi khi đóng; không sửa schema `.starc`.
- Xuất gói bàn giao và gửi bản sao vào Bàn viết web local qua thao tác người dùng.
- Không khởi chạy updater Jenny Script; không gửi crash/telemetry lên nhóm gốc trong cấu hình build này.

**Đã build Windows x64 thành công ngày 11/11/2026**, từ commit `4b975784df43b37f570b5554607c52ee91311d0c`.
[CI 42998147303](https://github.com/linhkaka/snakemedia-/actions/runs/42998147303) đạt kiểm tra nguồn,
Qt Test, biên dịch tiếng Việt, build app, đóng gói và thử khởi động headless 12 giây với PATH không có Qt.
Đây là bản portable đầu tiên; các thao tác giao diện/editor/Google vẫn cần nghiệm thu theo checklist bên dưới.
Máy hiện tại không có Qt/MSVC; bản build được tạo trên GitHub Actions tại [linhkaka/snakemedia-](https://github.com/linhkaka/snakemedia-).

## Build bằng GitHub Actions

Workflow: `.github/workflows/build_snakemedia.yml`.
Workflow không publish release, không cấp quyền ghi repo. Nó tạo artifact để tải về kiểm tra trước.
Nhánh `main` có bản workflow thủ công lấy nguồn từ `phase-snakemedia-desktop`; nhánh sản phẩm có thêm trigger push/PR.

1. Dùng nhánh `phase-snakemedia-desktop` trong repo `linhkaka/snakemedia-` đã được người dùng chọn.
2. Push nhánh `phase-snakemedia-desktop` sẽ chạy workflow; hoặc chạy thủ công **Build SNAKEMEDIA Studio**.
3. Các bước: lấy đúng ba submodule public → cấu hình thương hiệu/source URL → kiểm tra nguồn →
   cài Qt/MSVC/OpenSSL → compile và chạy Qt Test của hồ sơ → lrelease tiếng Việt → build app →
   windeployqt → thử khởi động gói bằng offscreen → tạo source archive và SHA256 → upload artifact.
4. Tải **SNAKEMEDIA-Studio-windows-x64**. Giải nén và mở `snakemedia-studio.exe`.
5. Tải kèm **SNAKEMEDIA-Studio-source** để giữ nguồn tương ứng. Kết quả test native có artifact riêng.

Giữ các workflow STARC cũ làm tài liệu; chỉ dùng workflow SNAKEMEDIA cho sản phẩm này.
Tất cả workflow Jenny Script/STARC kế thừa được chuyển sang chạy thủ công để không tự chạy khi push.
Không chạy các workflow upstream vốn cần private submodules.

Google Docs không bắt buộc. Nếu muốn bật đăng nhập Google, repo cần hai Secrets
`GOOGLE_OAUTH_CLIENT_ID`, `GOOGLE_OAUTH_CLIENT_SECRET` của một OAuth Desktop client riêng.
Không gửi giá trị secrets vào chat hoặc commit. Không dùng lại client/token từ Jenny Script.
Không có secrets thì build vẫn viết/xuất bình thường nhưng đăng nhập Google chưa hoạt động.

## Phạm vi kết nối AI

Desktop không nhúng private AI của STARC và không giữ API key của pipeline web.
**Đưa bản sao sang Bàn viết AI** gọi `http://127.0.0.1:8890/api/studio/import` khi người dùng bấm.
Nó tạo bản sao, mở Bàn viết web, không chạy AI hay tự đồng bộ `.starc`.
Cần chạy SNAKEMEDIA_VOICE riêng. Ghi chú nhân vật/bối cảnh được bàn giao như notes, chưa tự ép vào prompt.

## Kiểm thử và tiêu chí hoàn tất

- `python build/snakemedia/configure_brand.py`: tạo icon riêng, không cần thư viện ngoài.
- `python build/snakemedia/check_source.py`: kiểm tra thương hiệu, cách ly dữ liệu, catalog tiếng Việt,
  giữ GPL/credits và các bước kiểm thử/đóng gói trong CI. **Không thay thế C++ build.**
- `tests/snakemedia`: Qt Test cho tiếng Việt round-trip, dữ liệu sai, bảo toàn file cũ khi lưu lỗi,
  giới hạn file và JSON bàn giao.
- CI đã xanh. Gói EXE cần qua [checklist test tay](docs/HUONG_DAN_SNAKEMEDIA.txt) trước khi xác nhận phát hành chính thức.
- Chưa cài binary Jenny Script lên máy, chưa build Qt local, chưa thử OAuth thật.

## Các điểm móc khác upstream

Mã mới tập trung ở `src/core/management_layer/content/snakemedia/` và `src/include/snakemedia_brand.h`.
Điểm móc: `application.cpp`, `app.pro`, `app.rc`, `core.pro`, `resources.qrc`, `application_manager.cpp`,
`menu_view.{h,cpp}`, `about_application_dialog.cpp`, `onboarding_navigator.cpp`, `settings_storage.cpp`,
Google auth/docs strings + catalog VI, header version updater, include path test updater.
Không đổi `.gitmodules`, dữ liệu `.starc`, scope Google hoặc mã `src/3rd_party`.
