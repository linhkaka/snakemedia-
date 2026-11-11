# SNAKEMEDIA Studio Desktop

Bản desktop Windows x64 dựa trên Story Architect và Jenny Script, thêm thương hiệu và Bàn sản xuất SNAKEMEDIA. Giữ giấy phép GPL-3.0 và ghi công nguồn gốc.

- [Mã nguồn desktop](https://github.com/linhkaka/snakemedia-/tree/phase-snakemedia-desktop)
- [Phạm vi và hướng dẫn build](https://github.com/linhkaka/snakemedia-/blob/phase-snakemedia-desktop/README_SNAKEMEDIA.md)
- [Hướng dẫn sử dụng và checklist nghiệm thu](https://github.com/linhkaka/snakemedia-/blob/phase-snakemedia-desktop/docs/HUONG_DAN_SNAKEMEDIA.txt)
- [GitHub Actions](https://github.com/linhkaka/snakemedia-/actions)
- [Giấy phép](https://github.com/linhkaka/snakemedia-/blob/phase-snakemedia-desktop/LICENSE) · [Ghi công](https://github.com/linhkaka/snakemedia-/blob/phase-snakemedia-desktop/NOTICE.txt)

## Tình trạng

Đang chạy lượt build Windows đầu tiên; chưa xác nhận có EXE chạy được. Xem kết quả thực tế trong Actions.

Nhánh `main` chứa trang hướng dẫn và workflow khởi chạy thủ công. Mã sản phẩm nằm ở `phase-snakemedia-desktop`; workflow thủ công lấy mã từ nhánh này.

## Tải và mở bản build

Khi workflow thành công, tải artifact **SNAKEMEDIA-Studio-windows-x64**, giải nén toàn bộ rồi mở `snakemedia-studio.exe`. Giữ các DLL và thư mục plugin đi kèm. Tải thêm **SNAKEMEDIA-Studio-source** để giữ nguồn tương ứng.

Google Docs cần OAuth client riêng của SNAKEMEDIA. Kết nối AI cần hệ thống SNAKEMEDIA VOICE local tại `127.0.0.1:8890`; desktop không tự chạy AI.
