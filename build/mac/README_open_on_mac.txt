CÁCH CÀI JENNY SCRIPT TRÊN MAC

1. Kéo "Jenny Script" vào thư mục Applications.
2. Mở Applications, nhấp đúp "Jenny Script". Mac sẽ báo không xác minh được nhà phát triển.
   Bấm "Done" (hoặc "OK"), đừng bấm "Move to Trash".
3. Mở System Settings → Privacy & Security, kéo xuống phần Security, bấm "Open Anyway",
   rồi nhập mật khẩu máy. Chỉ cần làm một lần.

Mac cảnh báo vì bản này không trả phí chữ ký của Apple. Không có nghĩa là app có lỗi.

Nếu Mac báo app bị hỏng ("is damaged"), mở Terminal, dán lệnh sau rồi nhấn Enter, sau đó mở lại:

    xattr -dr com.apple.quarantine "/Applications/Jenny Script.app"

Bản Mac không tự cập nhật. Bản mới nằm ở đây:
https://github.com/vienglacial/starc/releases/latest

Trong app vẫn hiện tên "Story Architect". Xem nguồn gốc và giấy phép trong NOTICE.txt.


HOW TO INSTALL JENNY SCRIPT ON A MAC

1. Drag "Jenny Script" to the Applications folder.
2. Open Applications and double-click "Jenny Script". macOS says it cannot verify the developer.
   Click "Done" (or "OK"), not "Move to Trash".
3. Open System Settings → Privacy & Security, scroll down to Security, click "Open Anyway",
   and enter your password. You only need to do this once.

macOS warns because this build has no paid Apple signature. It does not mean the app is broken.

If macOS says the app "is damaged", open Terminal, paste this command, press Enter, and open it again:

    xattr -dr com.apple.quarantine "/Applications/Jenny Script.app"

The Mac version does not update itself. New versions are here:
https://github.com/vienglacial/starc/releases/latest

The app still shows the name "Story Architect". See NOTICE.txt for its origin and licences.
