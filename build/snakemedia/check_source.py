"""Fast source/package invariants; this is not a replacement for the C++ CI build."""
import re
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def main():
    checks = []

    def check(label, value):
        if not value:
            raise SystemExit("FAIL: " + label)
        checks.append(label)

    app = read("src/app/application.cpp")
    check("separate application settings identity", all("set" + key + "(SnakeMedia::" in app
          for key in ("ApplicationName", "OrganizationName", "OrganizationDomain")))
    settings = read("src/corelib/data_layer/storage/settings_storage.cpp")
    check("separate project and backup folders", "/SNAKEMEDIA/projects" in settings and "/SNAKEMEDIA/backups" in settings)
    manager = read("src/core/management_layer/application_manager.cpp")
    check("no automatic Jenny update invocation", "&AppUpdateManager::checkForUpdate" not in manager)
    check("production dialog connected to menu", "&Ui::MenuView::productionPressed" in manager and "dialog.exec()" in manager)
    check("production module included", "include(management_layer/content/snakemedia/snakemedia.pri)" in read("src/core/core.pro"))
    check("separate Google keychain service", 'kKeychainService = QStringLiteral("SNAKEMEDIA Studio")' in read("src/core/management_layer/content/google/google_auth_manager.cpp"))
    check("separate Drive root", 'kRootFolderName = QStringLiteral("SNAKEMEDIA Studio")' in read("src/core/management_layer/content/google/google_docs_manager.cpp"))
    ts = ET.parse(ROOT / "src/core/translations/translation_vi.ts").getroot()
    check("Vietnamese translation parses", ts.tag == "TS" and ts.attrib.get("language", "").startswith("vi"))
    check("Vietnamese translation catalog retained", len(ts.findall(".//message")) > 4000)
    check("native header source URL present", re.search(r'kSourceUrl\[\] = "(?:https://github.com/[\w.-]+/[\w.-]+)?";', read("src/include/snakemedia_brand.h")))
    check("native executable has its own name", "win32: TARGET = snakemedia-studio" in read("src/app/app.pro"))
    check("own PNG icon", (ROOT / "src/core/resources/images/snakemedia.png").read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))
    check("own Windows icon", (ROOT / "src/app/snakemedia.ico").read_bytes().startswith(b"\x00\x00\x01\x00"))
    original_license = subprocess.check_output(["git", "show", "HEAD:LICENSE"], cwd=ROOT)
    check("GPL license unchanged", original_license.replace(b"\r\n", b"\n") == (ROOT / "LICENSE").read_bytes().replace(b"\r\n", b"\n"))
    notice = read("NOTICE.txt")
    check("credits for both sources and SNAKEMEDIA", all(word in notice for word in ("SNAKEMEDIA", "Jenny Pixel Studio", "Story Apps")))
    workflow = read(".github/workflows/build_snakemedia.yml")
    check("CI initializes only public submodule paths", "--init --recursive src/3rd_party/pdfhummus src/3rd_party/pdftextextraction src/3rd_party/qtkeychain" in workflow)
    check("CI builds without Crashpad", "CONFIG+=no_crashpad" in workflow)
    check("CI tests native data", "tst_snakemedia.exe" in workflow)
    check("CI packages corresponding source", "package_source.py" in workflow and "SNAKEMEDIA-Studio-source" in workflow)
    for label in checks:
        print("PASS: " + label)
    print(f"{len(checks)} source checks passed. C++ compilation and GUI startup remain CI gates.")


if __name__ == "__main__":
    main()
