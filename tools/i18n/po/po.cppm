export module waywallen.i18n.po;

import rstd;

export namespace waywallen::i18n::po
{

using namespace rstd::prelude;
using ::alloc::string::String;
using ::alloc::vec::Vec;

enum class ErrorCode : rstd::uint8_t
{
    InvalidUtf8,
    InvalidLocale,
    Syntax,
    DuplicateEntry,
    InvalidHeader,
    LocaleMismatch,
    UnsupportedContext,
    UnsupportedPlural,
    EmptyTranslation,
    FuzzyTranslation,
    ObsoleteTranslation,
    SourceDrift,
};

auto code_name(ErrorCode code) noexcept -> ref<str>;

struct Error {
    ErrorCode code;
    usize     line;
    usize     column;
    String    message;
};

template<typename T>
using Result = rstd::Result<T, Error>;

struct Entry {
    Vec<String>    translator_comments;
    Vec<String>    extracted_comments;
    Vec<String>    references;
    Vec<String>    flags;
    Vec<String>    previous;
    Option<String> context;
    String         msgid;
    Option<String> msgid_plural;
    Vec<String>    msgstrs;
    bool           obsolete {};
    usize          line {};

    auto is_header() const noexcept -> bool;
    auto is_fuzzy() const noexcept -> bool;
};

struct Document {
    Vec<Entry> entries;
};

struct SourceMessage {
    String      msgid;
    Vec<String> references;
    Vec<String> extracted_comments;
};

struct Translation {
    String msgid;
    String msgstr;
};

struct RuntimeView {
    String           locale;
    Vec<Translation> translations;
};

auto parse(ref<str> text) -> Result<Document>;
auto canonicalize_locale(ref<str> locale) -> Result<String>;
auto render(const Document& document) -> String;
auto update(ref<str> project_id_version, ref<str> locale, Option<ref<str>> existing,
            slice<SourceMessage> messages) -> Result<String>;
auto check(ref<str> locale, const Document& document, slice<SourceMessage> messages)
    -> Result<empty>;
auto runtime_view(ref<str> locale, const Document& document) -> Result<RuntimeView>;

} // namespace waywallen::i18n::po
