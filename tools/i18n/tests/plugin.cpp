import rstd;
import waywallen.i18n.plugin;
import waywallen.i18n.po;

using namespace rstd::literals;
using namespace rstd::prelude;
using namespace waywallen::i18n;
using ::alloc::string::String;
using ::alloc::vec::Vec;

namespace
{

struct Checks {
    int failures {};

    void expect(bool condition, const char* message) {
        if (condition) return;
        ++failures;
        __builtin_printf("FAIL: plugin i18n %s\n", message);
    }
};

auto child(ref<rstd::path::Path> root, ref<str> name) -> rstd::path::PathBuf {
    return rstd::path::PathBuf::from(root).join(rstd::path::PathBuf::from(name).as_path());
}

auto write(ref<rstd::path::Path> root, ref<str> name, ref<str> contents) -> bool {
    auto path = child(root, name);
    return rstd::fs::write(path.as_path(), contents.as_bytes()).is_ok();
}

auto request(Mode mode, ref<rstd::path::Path> root, ref<str> locale) -> Request {
    auto locales = Vec<String>::make();
    locales.push(String::make(locale));
    return Request { mode, rstd::path::PathBuf::from(root), rstd::move(locales) };
}

void expect_workflow(Checks& checks) {
    auto temporary = rstd::fs::TempDir::make("waywallen-i18n-test"_str);
    checks.expect(temporary.is_ok(), "temporary plugin root should be created");
    if (temporary.is_err()) return;
    auto root = temporary->path();

    checks.expect(
        write(
            root,
            "plugin.toml"_str,
            "[plugin]\n"
            "id = \"org.test.i18n\"\n"
            "name = \"Test\"\n"
            "version = \"1.0\"\n"
            "\n"
            "[plugin.i18n]\n"
            "directory = \"i18n\"\n"
            "\n"
            "[renderers.test.settings]\n"
            "shared = { type = \"string\", default = \"\", label = { msgid = \"Shared\" } }\n"_str),
        "plugin manifest should be written");
    checks.expect(write(root, "main.lua"_str, "return { value = tr(\"Shared\", 503) }\n"_str),
                  "Lua source should be written");
    checks.expect(
        write(root, "files.txt"_str, "plugin.toml\nfiles.txt\nmain.lua\ni18n/ru.po\n"_str),
        "files manifest should be written");

    auto update_request = request(Mode::Update, root, "ru"_str);
    checks.expect(execute(update_request) == ExitCode::Success,
                  "update should merge Lua and manifest messages");
    auto po_path = child(root, "i18n/ru.po"_str);
    auto text    = rstd::fs::read_to_string(po_path.as_path());
    checks.expect(text.is_ok(), "updated PO should be readable");
    if (text.is_err()) return;
    auto document = po::parse(text->as_str());
    checks.expect(document.is_ok(), "updated PO should parse through the shared library");
    if (document.is_err()) return;
    checks.expect(document->entries.len() == usize(2),
                  "same msgid from Lua and manifest should merge into one entry");
    auto& entry = document->entries[usize(1)];
    checks.expect(entry.references.len() == usize(2),
                  "merged entry should retain both plugin-relative references");
    checks.expect(entry.references[usize {}] == "main.lua:1"_str &&
                      entry.references[usize(1)] == "plugin.toml"_str,
                  "references should be stable and plugin-relative");
    entry.msgstrs[usize {}] = String::make("Общее"_str);
    auto translated         = po::render(*document);
    checks.expect(rstd::fs::write(po_path.as_path(), translated.as_str().as_bytes()).is_ok(),
                  "translated PO should be written");

    auto check_request = request(Mode::Check, root, "ru"_str);
    checks.expect(execute(check_request) == ExitCode::Success,
                  "check should accept a synchronized translation");
    auto before = rstd::fs::read_to_string(po_path.as_path()).unwrap();
    checks.expect(execute(update_request) == ExitCode::Success,
                  "repeated update should remain successful");
    auto after = rstd::fs::read_to_string(po_path.as_path()).unwrap();
    checks.expect(before == after, "repeated update should be byte-identical");

    rstd::fs::remove_file(po_path.as_path()).unwrap();
    checks.expect(execute(check_request) == ExitCode::Translation,
                  "check should reject a declared but missing PO");

    checks.expect(write(root, "files.txt"_str, "plugin.toml\nfiles.txt\n../escape.lua\n"_str),
                  "unsafe files fixture should be written");
    checks.expect(execute(update_request) == ExitCode::Plugin,
                  "tool should reject an escaping declared Lua path");
}

} // namespace

auto main() -> int {
    Checks checks;
    expect_workflow(checks);
    return checks.failures;
}
