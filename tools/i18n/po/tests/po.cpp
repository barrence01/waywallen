import waywallen.i18n.po;
import rstd;

using namespace rstd::literals;
using namespace rstd::prelude;
using namespace waywallen::i18n::po;
using ::alloc::string::String;
using ::alloc::vec::Vec;

namespace
{

struct Checks {
    int failures {};

    void expect(bool condition, const char* message) {
        if (condition) return;
        ++failures;
        __builtin_printf("FAIL: po %s\n", message);
    }
};

auto message(ref<str> msgid, ref<str> reference) -> SourceMessage {
    auto references = Vec<String>::make();
    references.push(String::make(reference));
    return SourceMessage { String::make(msgid), rstd::move(references), Vec<String>::make() };
}

void expect_parse_and_runtime(Checks& checks) {
    auto parsed = parse("msgid \"\"\n"
                        "msgstr \"\"\n"
                        "\"Project-Id-Version: example 1.0\\n\"\n"
                        "\"Language: zh-CN\\n\"\n"
                        "\"MIME-Version: 1.0\\n\"\n"
                        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                        "\"Content-Transfer-Encoding: 8bit\\n\"\n"
                        "\n"
                        "#. Visible label\n"
                        "#: main.lua:2 plugin.toml\n"
                        "msgid \"Hel\"\n"
                        "\"lo\"\n"
                        "msgstr \"你\"\n"
                        "\"好\"\n"
                        "\n"
                        "#, fuzzy\n"
                        "msgid \"Pending\"\n"
                        "msgstr \"待定\"\n"_str);
    checks.expect(parsed.is_ok(), "standard multiline document should parse");
    if (parsed.is_err()) return;
    checks.expect(parsed->entries.len() == usize(3), "header and entries should be retained");
    auto view = runtime_view("zh-CN"_str, *parsed);
    checks.expect(view.is_ok(), "runtime view should validate generated headers");
    if (view.is_ok()) {
        checks.expect(view->translations.len() == usize(1),
                      "fuzzy translations should fall back instead of entering the view");
        checks.expect(view->translations[usize {}].msgid == "Hello"_str,
                      "continued msgid should concatenate");
        checks.expect(view->translations[usize {}].msgstr == "你好"_str,
                      "continued UTF-8 msgstr should concatenate");
    }
    auto rendered = render(*parsed);
    auto reparsed = parse(rendered.as_str());
    checks.expect(reparsed.is_ok(), "rendered PO should parse again");

    auto escaped = parse("msgid \"\"\n"
                         "msgstr \"Language: ru\\nContent-Type: text/plain; charset=UTF-8\\n\"\n\n"
                         "#| msgid \"Previous\"\n"
                         "msgid \"Quote: \\\" and slash: \\\\\"\n"
                         "msgstr \"Кавычка: \\\" и слэш: \\\\\"\n"_str);
    checks.expect(escaped.is_ok(), "escaped strings and previous msgid should parse");
    if (escaped.is_ok()) {
        checks.expect(escaped->entries[usize(1)].previous.len() == usize(1),
                      "previous msgid metadata should be retained");
        checks.expect(escaped->entries[usize(1)].msgid == "Quote: \" and slash: \\"_str,
                      "standard PO escapes should decode");
    }
}

void expect_update(Checks& checks) {
    auto messages = Vec<SourceMessage>::make();
    messages.push(message("Hello"_str, "main.lua:2"_str));
    auto created = update("org.example.plugin 1.0"_str, "zh-CN"_str, None(), messages.as_slice());
    checks.expect(created.is_ok(), "new PO should be generated");
    if (created.is_err()) return;
    auto translated = rstd::move(created).unwrap_unchecked();
    auto marker     = translated.as_str().find("msgstr \"\"\n"_str);
    checks.expect(marker.is_some(), "new message should have an empty translation");
    if (marker.is_none()) return;
    translated.replace_range(
        *marker, *marker + "msgstr \"\""_str.len(), "# Translator note\nmsgstr \"你好\""_str);

    auto updated = update(
        "org.example.plugin 1.0"_str, "zh-CN"_str, Some(translated.as_str()), messages.as_slice());
    checks.expect(updated.is_ok(), "existing PO should update");
    if (updated.is_err()) return;
    checks.expect(updated->as_str().contains("# Translator note"_str),
                  "translator comments should survive update");
    checks.expect(updated->as_str().contains("msgstr \"你好\""_str),
                  "translation should survive update");
    auto stable = update(
        "org.example.plugin 1.0"_str, "zh-CN"_str, Some(updated->as_str()), messages.as_slice());
    checks.expect(stable.is_ok() && stable->as_str() == updated->as_str(),
                  "a repeated update should be byte-identical");

    auto checked_document = parse(updated->as_str());
    checks.expect(checked_document.is_ok(), "updated PO should parse");
    if (checked_document.is_ok())
        checks.expect(check("zh-CN"_str, *checked_document, messages.as_slice()).is_ok(),
                      "complete synchronized PO should pass check");

    auto removed  = Vec<SourceMessage>::make();
    auto obsolete = update(
        "org.example.plugin 1.0"_str, "zh-CN"_str, Some(updated->as_str()), removed.as_slice());
    checks.expect(obsolete.is_ok() && obsolete->as_str().contains("#~ msgid \"Hello\""_str),
                  "removed messages should become obsolete");
}

void expect_policy(Checks& checks) {
    auto canonical = canonicalize_locale("zh_hans_cn"_str);
    checks.expect(canonical.is_ok() && canonical->as_str() == "zh-Hans-CN"_str,
                  "locale canonicalization should normalize separators and casing");
    checks.expect(canonicalize_locale("zh--CN"_str).is_err(),
                  "invalid locale syntax should be rejected");

    auto context =
        parse("msgid \"\"\nmsgstr \"Language: ru\\nContent-Type: text/plain; charset=UTF-8\\n\"\n\n"
              "msgctxt \"button\"\nmsgid \"Open\"\nmsgstr \"Открыть\"\n"_str);
    checks.expect(context.is_ok(), "context syntax should parse");
    if (context.is_ok()) {
        auto view = runtime_view("ru"_str, *context);
        checks.expect(view.is_err() && view.unwrap_err().code == ErrorCode::UnsupportedContext,
                      "runtime policy should reject context entries");
    }

    auto plural = parse(
        "msgid \"\"\nmsgstr \"Language: ru\\nContent-Type: text/plain; charset=UTF-8\\n\"\n\n"
        "msgid \"File\"\nmsgid_plural \"Files\"\nmsgstr[0] \"Файл\"\nmsgstr[1] \"Файлы\"\n"_str);
    checks.expect(plural.is_ok(), "plural syntax should parse");
    if (plural.is_ok()) {
        auto view = runtime_view("ru"_str, *plural);
        checks.expect(view.is_err() && view.unwrap_err().code == ErrorCode::UnsupportedPlural,
                      "runtime policy should reject plural entries");
    }

    auto duplicate = parse("msgid \"A\"\nmsgstr \"A\"\n\nmsgid \"A\"\nmsgstr \"B\"\n"_str);
    checks.expect(duplicate.is_err() && duplicate.unwrap_err().code == ErrorCode::DuplicateEntry,
                  "duplicate active entries should fail parsing");

    auto duplicate_empty = parse("msgid \"A\"\nmsgstr \"\"\nmsgstr \"\"\n"_str);
    checks.expect(duplicate_empty.is_err() &&
                      duplicate_empty.unwrap_err().code == ErrorCode::Syntax,
                  "duplicate empty translations should fail parsing");

    auto malformed_quote = parse("msgid \"A\"B\"\nmsgstr \"\"\n"_str);
    checks.expect(malformed_quote.is_err() &&
                      malformed_quote.unwrap_err().code == ErrorCode::Syntax,
                  "unescaped quotes should fail parsing");

    auto messages = Vec<SourceMessage>::make();
    messages.push(message("Ready"_str, "main.lua:1"_str));
    auto empty = parse("msgid \"\"\n"
                       "msgstr \"Language: ru\\nContent-Type: text/plain; charset=UTF-8\\n\"\n\n"
                       "#: main.lua:1\nmsgid \"Ready\"\nmsgstr \"\"\n"_str);
    checks.expect(empty.is_ok(), "empty translation fixture should parse");
    if (empty.is_ok()) {
        auto checked = check("ru"_str, *empty, messages.as_slice());
        checks.expect(checked.is_err() && checked.unwrap_err().code == ErrorCode::EmptyTranslation,
                      "check should reject empty translations");
    }

    auto fuzzy = parse("msgid \"\"\n"
                       "msgstr \"Language: ru\\nContent-Type: text/plain; charset=UTF-8\\n\"\n\n"
                       "#: main.lua:1\n#, fuzzy\nmsgid \"Ready\"\nmsgstr \"Готово\"\n"_str);
    checks.expect(fuzzy.is_ok(), "fuzzy translation fixture should parse");
    if (fuzzy.is_ok()) {
        auto checked = check("ru"_str, *fuzzy, messages.as_slice());
        checks.expect(checked.is_err() && checked.unwrap_err().code == ErrorCode::FuzzyTranslation,
                      "check should reject fuzzy translations");
    }

    auto obsolete = parse("msgid \"\"\n"
                          "msgstr \"Language: ru\\nContent-Type: text/plain; charset=UTF-8\\n\"\n\n"
                          "#~ msgid \"Old\"\n#~ msgstr \"Старое\"\n"_str);
    checks.expect(obsolete.is_ok(), "obsolete translation fixture should parse");
    if (obsolete.is_ok()) {
        auto checked = check("ru"_str, *obsolete, messages.as_slice());
        checks.expect(checked.is_err() &&
                          checked.unwrap_err().code == ErrorCode::ObsoleteTranslation,
                      "check should reject obsolete translations");
    }

    auto wrong_locale = parse(
        "msgid \"\"\nmsgstr \"Language: zh-CN\\nContent-Type: text/plain; charset=UTF-8\\n\"\n"_str);
    checks.expect(wrong_locale.is_ok(), "locale mismatch fixture should parse");
    if (wrong_locale.is_ok()) {
        auto view = runtime_view("ru"_str, *wrong_locale);
        checks.expect(view.is_err() && view.unwrap_err().code == ErrorCode::LocaleMismatch,
                      "runtime should reject a mismatched PO locale");
    }

    auto wrong_charset = parse(
        "msgid \"\"\nmsgstr \"Language: ru\\nContent-Type: text/plain; charset=KOI8-R\\n\"\n"_str);
    checks.expect(wrong_charset.is_ok(), "charset fixture should parse");
    if (wrong_charset.is_ok()) {
        auto view = runtime_view("ru"_str, *wrong_charset);
        checks.expect(view.is_err() && view.unwrap_err().code == ErrorCode::InvalidHeader,
                      "runtime should reject non-UTF-8 PO content declarations");
    }
}

} // namespace

auto main() -> int {
    Checks checks;
    expect_parse_and_runtime(checks);
    expect_update(checks);
    expect_policy(checks);
    return checks.failures;
}
