module waywallen.i18n.po;

namespace waywallen::i18n::po
{

using namespace rstd::literals;
using ::alloc::collections::BTreeMap;

namespace
{

enum class Field
{
    None,
    Context,
    Message,
    Plural,
    Translation,
};

struct PendingEntry {
    Entry      entry;
    Vec<usize> translation_fields;
    Field      field { Field::None };
    usize      translation_index {};
    bool       has_fields {};
    bool       has_context {};
    bool       has_msgid {};
    bool       has_plural {};
};

auto error(ErrorCode code, usize line, usize column, ref<str> message) -> Error {
    return Error { code, line, column, String::make(message) };
}

auto starts_with_ascii_case_insensitive(ref<str> value, ref<str> prefix) noexcept -> bool {
    if (value.len() < prefix.len()) return false;
    for (usize index {}; index < prefix.len(); ++index) {
        auto left  = value.as_bytes()[index];
        auto right = prefix.as_bytes()[index];
        if (left >= u8('A') && left <= u8('Z')) left = left - u8('A') + u8('a');
        if (right >= u8('A') && right <= u8('Z')) right = right - u8('A') + u8('a');
        if (left != right) return false;
    }
    return true;
}

auto contains_ascii_case_insensitive(ref<str> value, ref<str> needle) noexcept -> bool {
    if (needle.is_empty()) return true;
    if (needle.len() > value.len()) return false;
    for (usize offset {}; offset + needle.len() <= value.len(); ++offset) {
        if (starts_with_ascii_case_insensitive(value.get(offset, value.len()).unwrap(), needle))
            return true;
    }
    return false;
}

auto canonical_locale_impl(ref<str> locale) -> String {
    auto result = String::make();
    auto begin  = usize {};
    auto part   = usize {};
    while (begin <= locale.len()) {
        auto end = begin;
        while (end < locale.len() && locale.as_bytes()[end] != u8('-') &&
               locale.as_bytes()[end] != u8('_'))
            ++end;
        auto segment = locale.get(begin, end).unwrap();
        if (segment.is_empty()) return String::make();
        if (part != usize {}) result.push_ascii('-');
        for (usize index {}; index < segment.len(); ++index) {
            auto value = segment.as_bytes()[index];
            if (! rstd::ascii::is_alnum(value)) return String::make();
            const bool script =
                part != usize {} && segment.len() == usize(4) && rstd::ascii::is_alpha(value);
            const bool region =
                part != usize {} && segment.len() == usize(2) && rstd::ascii::is_alpha(value);
            if (script && index == usize {} && value >= u8('a') && value <= u8('z'))
                value = value - u8('a') + u8('A');
            else if (region && value >= u8('a') && value <= u8('z'))
                value = value - u8('a') + u8('A');
            else if ((! script || index != usize {}) && ! region && value >= u8('A') &&
                     value <= u8('Z'))
                value = value - u8('A') + u8('a');
            result.push_ascii(value);
        }
        ++part;
        if (end == locale.len()) break;
        begin = end + usize(1);
    }
    return result;
}

auto quoted(ref<str> value, usize line, usize column) -> Result<String> {
    auto input = value.trim_ascii();
    if (input.len() < usize(2) || input.as_bytes()[usize {}] != u8('"') ||
        input.as_bytes()[input.len() - usize(1)] != u8('"')) {
        return Err(error(
            ErrorCode::Syntax, line, column, "PO directive must contain a quoted string"_str));
    }
    auto bytes = Vec<u8>::make();
    for (usize index { 1 }; index + usize(1) < input.len(); ++index) {
        auto value = input.as_bytes()[index];
        if (value != u8('\\')) {
            if (value == u8('"'))
                return Err(error(
                    ErrorCode::Syntax, line, column + index, "unescaped quote in PO string"_str));
            bytes.push(rstd::move(value));
            continue;
        }
        ++index;
        if (index + usize(1) >= input.len())
            return Err(error(ErrorCode::Syntax, line, column, "unfinished PO escape"_str));
        auto escaped = input.as_bytes()[index];
        switch (escaped.to_primitive()) {
        case 'a': bytes.push(u8('\a')); break;
        case 'b': bytes.push(u8('\b')); break;
        case 'f': bytes.push(u8('\f')); break;
        case 'n': bytes.push(u8('\n')); break;
        case 'r': bytes.push(u8('\r')); break;
        case 't': bytes.push(u8('\t')); break;
        case 'v': bytes.push(u8('\v')); break;
        case '\\': bytes.push(u8('\\')); break;
        case '"': bytes.push(u8('"')); break;
        default:
            if (escaped == u8('x')) {
                auto value = rstd::uint16_t {};
                auto count = usize {};
                while (index + usize(1) < input.len() &&
                       rstd::ascii::is_hex_digit(input.as_bytes()[index + usize(1)])) {
                    auto digit = *rstd::ascii::digit_value(input.as_bytes()[++index], u8(16));
                    value      = static_cast<rstd::uint16_t>(value * 16 + digit.to_primitive());
                    ++count;
                    if (value > 255)
                        return Err(error(
                            ErrorCode::Syntax, line, column, "PO hex escape exceeds one byte"_str));
                }
                if (count == usize {})
                    return Err(
                        error(ErrorCode::Syntax, line, column, "PO hex escape has no digits"_str));
                bytes.push(u8(static_cast<rstd::uint8_t>(value)));
            } else if (escaped >= u8('0') && escaped <= u8('7')) {
                auto value = static_cast<rstd::uint16_t>((escaped - u8('0')).to_primitive());
                auto count = usize(1);
                while (count < usize(3) && index + usize(1) < input.len() &&
                       input.as_bytes()[index + usize(1)] >= u8('0') &&
                       input.as_bytes()[index + usize(1)] <= u8('7')) {
                    value = static_cast<rstd::uint16_t>(
                        value * 8 + (input.as_bytes()[++index] - u8('0')).to_primitive());
                    ++count;
                }
                if (value > 255)
                    return Err(error(
                        ErrorCode::Syntax, line, column, "PO octal escape exceeds one byte"_str));
                bytes.push(u8(static_cast<rstd::uint8_t>(value)));
            } else {
                return Err(error(ErrorCode::Syntax, line, column, "unknown PO string escape"_str));
            }
        }
    }
    auto decoded = String::from_utf8(rstd::move(bytes));
    if (decoded.is_err())
        return Err(
            error(ErrorCode::InvalidUtf8, line, column, "PO quoted string is not valid UTF-8"_str));
    return Ok(rstd::move(decoded).unwrap_unchecked());
}

auto contains(const Vec<String>& values, ref<str> expected) noexcept -> bool {
    for (const auto& value : values) {
        if (value.as_str() == expected) return true;
    }
    return false;
}

void push_unique(Vec<String>& values, String value) {
    if (! contains(values, value.as_str())) values.push(rstd::move(value));
}

void split_words(ref<str> value, Vec<String>& output, bool commas) {
    auto begin = usize {};
    while (begin < value.len()) {
        while (begin < value.len() && (rstd::ascii::is_space(value.as_bytes()[begin]) ||
                                       (commas && value.as_bytes()[begin] == u8(','))))
            ++begin;
        if (begin == value.len()) break;
        auto end = begin;
        while (end < value.len() && ! rstd::ascii::is_space(value.as_bytes()[end]) &&
               (! commas || value.as_bytes()[end] != u8(',')))
            ++end;
        push_unique(output, String::make(value.get(begin, end).unwrap()));
        begin = end;
    }
}

auto append_value(PendingEntry& pending, Field field, usize index, String value, usize line)
    -> Result<empty> {
    pending.field             = field;
    pending.translation_index = index;
    switch (field) {
    case Field::Context:
        if (pending.has_context)
            return Err(error(ErrorCode::Syntax, line, usize(1), "duplicate msgctxt directive"_str));
        pending.has_context   = true;
        pending.entry.context = Some(rstd::move(value));
        break;
    case Field::Message:
        if (pending.has_msgid)
            return Err(error(ErrorCode::Syntax, line, usize(1), "duplicate msgid directive"_str));
        pending.has_msgid   = true;
        pending.entry.msgid = rstd::move(value);
        break;
    case Field::Plural:
        if (pending.has_plural)
            return Err(
                error(ErrorCode::Syntax, line, usize(1), "duplicate msgid_plural directive"_str));
        pending.has_plural         = true;
        pending.entry.msgid_plural = Some(rstd::move(value));
        break;
    case Field::Translation:
        for (const auto existing : pending.translation_fields)
            if (existing == index)
                return Err(
                    error(ErrorCode::Syntax, line, usize(1), "duplicate msgstr directive"_str));
        pending.translation_fields.push(usize(index.to_primitive()));
        while (pending.entry.msgstrs.len() <= index) pending.entry.msgstrs.push(String::make());
        pending.entry.msgstrs[index] = rstd::move(value);
        break;
    case Field::None: rstd::unreachable();
    }
    pending.has_fields = true;
    return Ok(empty {});
}

auto continue_value(PendingEntry& pending, String value, usize line) -> Result<empty> {
    switch (pending.field) {
    case Field::Context: pending.entry.context->push_str(value.as_str()); break;
    case Field::Message: pending.entry.msgid.push_str(value.as_str()); break;
    case Field::Plural: pending.entry.msgid_plural->push_str(value.as_str()); break;
    case Field::Translation:
        pending.entry.msgstrs[pending.translation_index].push_str(value.as_str());
        break;
    case Field::None:
        return Err(error(ErrorCode::Syntax, line, usize(1), "orphan PO quoted continuation"_str));
    }
    return Ok(empty {});
}

auto same_identity(const Entry& left, const Entry& right) noexcept -> bool {
    const auto left_context  = left.context.is_some() ? left.context->as_str() : ""_str;
    const auto right_context = right.context.is_some() ? right.context->as_str() : ""_str;
    return left_context == right_context && left.msgid == right.msgid;
}

auto finish(PendingEntry& pending, Document& document) -> Result<empty> {
    if (! pending.has_fields) {
        pending = PendingEntry {};
        return Ok(empty {});
    }
    if (! pending.has_msgid)
        return Err(error(
            ErrorCode::Syntax, pending.entry.line, usize(1), "PO entry is missing msgid"_str));
    if (pending.entry.msgstrs.is_empty())
        return Err(error(
            ErrorCode::Syntax, pending.entry.line, usize(1), "PO entry is missing msgstr"_str));
    if (! pending.entry.obsolete) {
        for (const auto& existing : document.entries) {
            if (! existing.obsolete && same_identity(existing, pending.entry))
                return Err(error(ErrorCode::DuplicateEntry,
                                 pending.entry.line,
                                 usize(1),
                                 "duplicate active PO entry"_str));
        }
    }
    document.entries.push(rstd::move(pending.entry));
    pending = PendingEntry {};
    return Ok(empty {});
}

auto parse_translation_index(ref<str> name, usize line) -> Result<usize> {
    if (name == "msgstr"_str) return Ok(usize {});
    if (! name.starts_with("msgstr["_str) || ! name.ends_with("]"_str))
        return Err(error(ErrorCode::Syntax, line, usize(1), "invalid msgstr directive"_str));
    auto digits = name.get(usize(7), name.len() - usize(1)).unwrap();
    if (digits.is_empty())
        return Err(error(ErrorCode::Syntax, line, usize(1), "empty msgstr index"_str));
    auto index = usize {};
    for (auto value : digits.as_bytes()) {
        if (! rstd::ascii::is_digit(value))
            return Err(error(ErrorCode::Syntax, line, usize(1), "invalid msgstr index"_str));
        index = index * usize(10) + usize((value - u8('0')).to_primitive());
    }
    return Ok(index);
}

auto header_fields(const Entry& header) -> Result<BTreeMap<String, String>> {
    auto fields = BTreeMap<String, String>::make();
    if (header.msgstrs.is_empty())
        return Err(
            error(ErrorCode::InvalidHeader, header.line, usize(1), "PO header has no msgstr"_str));
    auto text  = header.msgstrs[usize {}].as_str();
    auto begin = usize {};
    while (begin <= text.len()) {
        auto end = begin;
        while (end < text.len() && text.as_bytes()[end] != u8('\n')) ++end;
        auto line = text.get(begin, end).unwrap();
        if (! line.is_empty()) {
            auto separator = usize {};
            while (separator < line.len() && line.as_bytes()[separator] != u8(':')) ++separator;
            if (separator == line.len())
                return Err(error(ErrorCode::InvalidHeader,
                                 header.line,
                                 usize(1),
                                 "invalid PO header field"_str));
            auto name  = line.get(usize {}, separator).unwrap().trim_ascii();
            auto value = line.get(separator + usize(1), line.len()).unwrap().trim_ascii();
            if (name.is_empty() || fields.insert(String::make(name), String::make(value)).is_some())
                return Err(error(ErrorCode::InvalidHeader,
                                 header.line,
                                 usize(1),
                                 "duplicate or empty PO header field"_str));
        }
        if (end == text.len()) break;
        begin = end + usize(1);
    }
    return Ok(rstd::move(fields));
}

auto find_header(const Document& document) -> Result<ref<Entry>> {
    const Entry* header {};
    for (const auto& entry : document.entries) {
        if (entry.obsolete || ! entry.is_header()) continue;
        if (header != nullptr)
            return Err(
                error(ErrorCode::InvalidHeader, entry.line, usize(1), "multiple PO headers"_str));
        header = rstd::addressof(entry);
    }
    if (header == nullptr)
        return Err(error(ErrorCode::InvalidHeader, usize(1), usize(1), "missing PO header"_str));
    return Ok(ref<Entry>::from_raw_parts(header));
}

auto validate_header(ref<str> expected_locale, const Document& document) -> Result<String> {
    auto header = find_header(document);
    if (header.is_err()) return Err(rstd::move(header).unwrap_err_unchecked());
    auto fields = header_fields(**header);
    if (fields.is_err()) return Err(rstd::move(fields).unwrap_err_unchecked());
    auto language = fields->get("Language"_str);
    auto content  = fields->get("Content-Type"_str);
    if (language.is_none() || content.is_none())
        return Err(error(ErrorCode::InvalidHeader,
                         (*header)->line,
                         usize(1),
                         "PO header requires Language and Content-Type"_str));
    if (! contains_ascii_case_insensitive((*content)->as_str(), "charset=UTF-8"_str))
        return Err(error(ErrorCode::InvalidHeader,
                         (*header)->line,
                         usize(1),
                         "PO Content-Type charset must be UTF-8"_str));
    auto expected = canonical_locale_impl(expected_locale);
    auto actual   = canonical_locale_impl((*language)->as_str());
    if (expected.is_empty() || actual.is_empty() || expected != actual.as_str())
        return Err(error(ErrorCode::LocaleMismatch,
                         (*header)->line,
                         usize(1),
                         "PO Language does not match the requested locale"_str));
    return Ok(rstd::move(actual));
}

void append_quoted(String& output, ref<str> value) {
    output.push_ascii('"');
    auto begin = usize {};
    for (usize index {}; index < value.len(); ++index) {
        const auto byte    = value.as_bytes()[index];
        const bool escaped = byte == u8('"') || byte == u8('\\') || byte == u8('\n') ||
                             byte == u8('\r') || byte == u8('\t') || byte == u8('\b') ||
                             byte == u8('\f') || byte < u8(0x20);
        if (! escaped) continue;
        if (index != begin) output.push_str(value.get(begin, index).unwrap());
        output.push_ascii('\\');
        switch (byte.to_primitive()) {
        case '"': output.push_ascii('"'); break;
        case '\\': output.push_ascii('\\'); break;
        case '\n': output.push_ascii('n'); break;
        case '\r': output.push_ascii('r'); break;
        case '\t': output.push_ascii('t'); break;
        case '\b': output.push_ascii('b'); break;
        case '\f': output.push_ascii('f'); break;
        default:
            output.push_ascii(char('0' + ((byte.to_primitive() >> 6) & 7)));
            output.push_ascii(char('0' + ((byte.to_primitive() >> 3) & 7)));
            output.push_ascii(char('0' + (byte.to_primitive() & 7)));
            break;
        }
        begin = index + usize(1);
    }
    if (begin != value.len()) output.push_str(value.get(begin, value.len()).unwrap());
    output.push_ascii('"');
}

void append_line(String& output, bool obsolete, ref<str> directive, ref<str> value) {
    if (obsolete) output.push_str("#~ "_str);
    output.push_str(directive);
    output.push_ascii(' ');
    append_quoted(output, value);
    output.push_ascii('\n');
}

void render_entry(String& output, const Entry& entry) {
    for (const auto& comment : entry.translator_comments) {
        output.push_str("# "_str);
        output.push_str(comment.as_str());
        output.push_ascii('\n');
    }
    for (const auto& comment : entry.extracted_comments) {
        output.push_str("#. "_str);
        output.push_str(comment.as_str());
        output.push_ascii('\n');
    }
    if (! entry.references.is_empty()) {
        output.push_str("#: "_str);
        for (usize index {}; index < entry.references.len(); ++index) {
            if (index != usize {}) output.push_ascii(' ');
            output.push_str(entry.references[index].as_str());
        }
        output.push_ascii('\n');
    }
    if (! entry.flags.is_empty()) {
        output.push_str("#, "_str);
        for (usize index {}; index < entry.flags.len(); ++index) {
            if (index != usize {}) output.push_str(", "_str);
            output.push_str(entry.flags[index].as_str());
        }
        output.push_ascii('\n');
    }
    for (const auto& previous : entry.previous) {
        output.push_str("#| "_str);
        output.push_str(previous.as_str());
        output.push_ascii('\n');
    }
    if (entry.context.is_some())
        append_line(output, entry.obsolete, "msgctxt"_str, entry.context->as_str());
    append_line(output, entry.obsolete, "msgid"_str, entry.msgid.as_str());
    if (entry.msgid_plural.is_some())
        append_line(output, entry.obsolete, "msgid_plural"_str, entry.msgid_plural->as_str());
    for (usize index {}; index < entry.msgstrs.len(); ++index) {
        auto directive = entry.msgstrs.len() == usize(1) ? String::make("msgstr"_str)
                                                         : rstd::format("msgstr[{}]", index);
        append_line(output, entry.obsolete, directive.as_str(), entry.msgstrs[index].as_str());
    }
}

auto clone_entry(const Entry& entry) -> Entry {
    auto translator_comments = Vec<String>::with_capacity(entry.translator_comments.len());
    auto extracted_comments  = Vec<String>::with_capacity(entry.extracted_comments.len());
    auto references          = Vec<String>::with_capacity(entry.references.len());
    auto flags               = Vec<String>::with_capacity(entry.flags.len());
    auto previous            = Vec<String>::with_capacity(entry.previous.len());
    auto msgstrs             = Vec<String>::with_capacity(entry.msgstrs.len());
    for (const auto& value : entry.translator_comments) translator_comments.push(value.clone());
    for (const auto& value : entry.extracted_comments) extracted_comments.push(value.clone());
    for (const auto& value : entry.references) references.push(value.clone());
    for (const auto& value : entry.flags) flags.push(value.clone());
    for (const auto& value : entry.previous) previous.push(value.clone());
    for (const auto& value : entry.msgstrs) msgstrs.push(value.clone());
    return Entry { rstd::move(translator_comments),
                   rstd::move(extracted_comments),
                   rstd::move(references),
                   rstd::move(flags),
                   rstd::move(previous),
                   entry.context.is_some() ? Some(entry.context->clone()) : None(),
                   entry.msgid.clone(),
                   entry.msgid_plural.is_some() ? Some(entry.msgid_plural->clone()) : None(),
                   rstd::move(msgstrs),
                   entry.obsolete,
                   entry.line };
}

auto clone_strings(const Vec<String>& values) -> Vec<String> {
    auto output = Vec<String>::with_capacity(values.len());
    for (const auto& value : values) output.push(value.clone());
    return output;
}

auto normalized_messages(slice<SourceMessage> messages) -> BTreeMap<String, SourceMessage> {
    auto output = BTreeMap<String, SourceMessage>::make();
    for (const auto& message : messages) {
        auto existing = output.get_mut(message.msgid.as_str());
        if (existing.is_none()) {
            output.insert(message.msgid.clone(),
                          SourceMessage { message.msgid.clone(),
                                          clone_strings(message.references),
                                          clone_strings(message.extracted_comments) });
            continue;
        }
        for (const auto& reference : message.references)
            push_unique((*existing)->references, reference.clone());
        for (const auto& comment : message.extracted_comments)
            push_unique((*existing)->extracted_comments, comment.clone());
    }
    return output;
}

auto build_header(ref<str> project, ref<str> locale, const Entry* previous) -> Result<Entry> {
    auto fields = BTreeMap<String, String>::make();
    if (previous != nullptr) {
        auto parsed = header_fields(*previous);
        if (parsed.is_err()) return Err(rstd::move(parsed).unwrap_err_unchecked());
        fields = rstd::move(parsed).unwrap_unchecked();
    }
    fields.insert(String::make("Project-Id-Version"_str), String::make(project));
    fields.insert(String::make("Language"_str), canonical_locale_impl(locale));
    fields.insert(String::make("MIME-Version"_str), String::make("1.0"_str));
    fields.insert(String::make("Content-Type"_str), String::make("text/plain; charset=UTF-8"_str));
    fields.insert(String::make("Content-Transfer-Encoding"_str), String::make("8bit"_str));

    static constexpr auto preferred = rstd::array<ref<str>, 5> {
        "Project-Id-Version"_str,        "Language"_str, "MIME-Version"_str, "Content-Type"_str,
        "Content-Transfer-Encoding"_str,
    };
    auto text = String::make();
    for (auto name : preferred) {
        auto value = fields.remove(name).unwrap();
        text.push_str(name);
        text.push_str(": "_str);
        text.push_str(value.as_str());
        text.push_ascii('\n');
    }
    while (auto item = fields.pop_first()) {
        text.push_str(item->template get<0>().as_str());
        text.push_str(": "_str);
        text.push_str(item->template get<1>().as_str());
        text.push_ascii('\n');
    }
    auto msgstrs = Vec<String>::make();
    msgstrs.push(rstd::move(text));
    auto comments =
        previous == nullptr ? Vec<String>::make() : clone_strings(previous->translator_comments);
    return Ok(Entry { rstd::move(comments),
                      Vec<String>::make(),
                      Vec<String>::make(),
                      Vec<String>::make(),
                      Vec<String>::make(),
                      None(),
                      String::make(),
                      None(),
                      rstd::move(msgstrs),
                      false,
                      usize(1) });
}

auto references_equal(const Vec<String>& left, const Vec<String>& right) noexcept -> bool {
    if (left.len() != right.len()) return false;
    for (const auto& value : left) {
        if (! contains(right, value.as_str())) return false;
    }
    return true;
}

} // namespace

auto Entry::is_header() const noexcept -> bool {
    return context.is_none() && msgid.is_empty() && msgid_plural.is_none();
}

auto Entry::is_fuzzy() const noexcept -> bool { return contains(flags, "fuzzy"_str); }

auto canonicalize_locale(ref<str> locale) -> Result<String> {
    auto canonical = canonical_locale_impl(locale);
    if (canonical.is_empty())
        return Err(error(ErrorCode::InvalidLocale,
                         usize(1),
                         usize(1),
                         "locale must be a non-empty BCP 47 language tag"_str));
    return Ok(rstd::move(canonical));
}

auto parse(ref<str> text) -> Result<Document> {
    if (rstd::str_::from_utf8(text.as_bytes()).is_err())
        return Err(error(
            ErrorCode::InvalidUtf8, usize(1), usize(1), "PO document is not valid UTF-8"_str));
    auto document = Document { Vec<Entry>::make() };
    auto pending  = PendingEntry {};
    auto begin    = usize {};
    auto line     = usize(1);
    while (begin <= text.len()) {
        auto end = begin;
        while (end < text.len() && text.as_bytes()[end] != u8('\n')) ++end;
        auto line_end = end;
        if (line_end > begin && text.as_bytes()[line_end - usize(1)] == u8('\r')) --line_end;
        auto raw = text.get(begin, line_end).unwrap();
        if (raw.trim_ascii().is_empty()) {
            auto completed = finish(pending, document);
            if (completed.is_err()) return Err(rstd::move(completed).unwrap_err_unchecked());
        } else {
            if (pending.entry.line == usize {}) pending.entry.line = line;
            auto content  = raw;
            bool obsolete = false;
            if (content.starts_with("#~"_str)) {
                obsolete               = true;
                content                = content.get(usize(2), content.len()).unwrap().trim_ascii();
                pending.entry.obsolete = true;
            }
            if (content.starts_with("#|"_str)) {
                pending.entry.previous.push(
                    String::make(content.get(usize(2), content.len()).unwrap().trim_ascii()));
                pending.field = Field::None;
            } else if (content.starts_with("#."_str)) {
                pending.entry.extracted_comments.push(
                    String::make(content.get(usize(2), content.len()).unwrap().trim_ascii()));
                pending.field = Field::None;
            } else if (content.starts_with("#:"_str)) {
                split_words(
                    content.get(usize(2), content.len()).unwrap(), pending.entry.references, false);
                pending.field = Field::None;
            } else if (content.starts_with("#,"_str)) {
                split_words(
                    content.get(usize(2), content.len()).unwrap(), pending.entry.flags, true);
                pending.field = Field::None;
            } else if (content.starts_with("#"_str)) {
                auto comment = content.get(usize(1), content.len()).unwrap();
                if (! comment.is_empty() && comment.as_bytes()[usize {}] == u8(' '))
                    comment = comment.get(usize(1), comment.len()).unwrap();
                pending.entry.translator_comments.push(String::make(comment));
                pending.field = Field::None;
            } else {
                auto separator = usize {};
                while (separator < content.len() &&
                       ! rstd::ascii::is_space(content.as_bytes()[separator]))
                    ++separator;
                auto name       = content.get(usize {}, separator).unwrap();
                auto value_text = content.get(separator, content.len()).unwrap().trim_ascii();
                if (name.is_empty() || name.as_bytes()[usize {}] == u8('"')) {
                    auto continued = quoted(content, line, usize(1));
                    if (continued.is_err())
                        return Err(rstd::move(continued).unwrap_err_unchecked());
                    auto appended =
                        continue_value(pending, rstd::move(continued).unwrap_unchecked(), line);
                    if (appended.is_err()) return Err(rstd::move(appended).unwrap_err_unchecked());
                } else {
                    auto value = quoted(value_text, line, separator + usize(1));
                    if (value.is_err()) return Err(rstd::move(value).unwrap_err_unchecked());
                    Field field = Field::None;
                    auto  index = usize {};
                    if (name == "msgctxt"_str)
                        field = Field::Context;
                    else if (name == "msgid"_str)
                        field = Field::Message;
                    else if (name == "msgid_plural"_str)
                        field = Field::Plural;
                    else if (name.starts_with("msgstr"_str)) {
                        field             = Field::Translation;
                        auto parsed_index = parse_translation_index(name, line);
                        if (parsed_index.is_err())
                            return Err(rstd::move(parsed_index).unwrap_err_unchecked());
                        index = rstd::move(parsed_index).unwrap_unchecked();
                    } else {
                        return Err(
                            error(ErrorCode::Syntax, line, usize(1), "unknown PO directive"_str));
                    }
                    auto appended = append_value(
                        pending, field, index, rstd::move(value).unwrap_unchecked(), line);
                    if (appended.is_err()) return Err(rstd::move(appended).unwrap_err_unchecked());
                }
            }
            if (obsolete) pending.entry.obsolete = true;
        }
        if (end == text.len()) break;
        begin = end + usize(1);
        ++line;
    }
    auto completed = finish(pending, document);
    if (completed.is_err()) return Err(rstd::move(completed).unwrap_err_unchecked());
    return Ok(rstd::move(document));
}

auto render(const Document& document) -> String {
    const Entry* header {};
    auto         active   = BTreeMap<String, ref<Entry>>::make();
    auto         obsolete = BTreeMap<String, ref<Entry>>::make();
    for (const auto& entry : document.entries) {
        if (! entry.obsolete && entry.is_header()) {
            header = rstd::addressof(entry);
            continue;
        }
        auto key = entry.context.is_some()
                       ? rstd::format("{}\x1f{}", entry.context->as_str(), entry.msgid.as_str())
                       : entry.msgid.clone();
        if (entry.obsolete)
            obsolete.insert(rstd::move(key), ref<Entry>::from_raw_parts(rstd::addressof(entry)));
        else
            active.insert(rstd::move(key), ref<Entry>::from_raw_parts(rstd::addressof(entry)));
    }
    auto output = String::make();
    auto append = [&](const Entry& entry) {
        if (! output.is_empty()) output.push_ascii('\n');
        render_entry(output, entry);
    };
    if (header != nullptr) {
        append(*header);
    }
    for (auto item : active.iter()) {
        append(**item.template get<1>());
    }
    for (auto item : obsolete.iter()) {
        append(**item.template get<1>());
    }
    return output;
}

auto update(ref<str> project_id_version, ref<str> locale, Option<ref<str>> existing,
            slice<SourceMessage> messages) -> Result<String> {
    auto canonical = canonicalize_locale(locale);
    if (canonical.is_err()) return Err(rstd::move(canonical).unwrap_err_unchecked());
    auto previous = Document { Vec<Entry>::make() };
    if (existing.is_some()) {
        auto parsed = parse(*existing);
        if (parsed.is_err()) return Err(rstd::move(parsed).unwrap_err_unchecked());
        previous       = rstd::move(parsed).unwrap_unchecked();
        auto validated = validate_header(canonical->as_str(), previous);
        if (validated.is_err()) return Err(rstd::move(validated).unwrap_err_unchecked());
    }

    const Entry* old_header {};
    auto         active   = BTreeMap<String, Entry>::make();
    auto         obsolete = BTreeMap<String, Entry>::make();
    for (const auto& entry : previous.entries) {
        if (! entry.obsolete && entry.is_header()) {
            old_header = rstd::addressof(entry);
            continue;
        }
        if (entry.context.is_some())
            return Err(error(ErrorCode::UnsupportedContext,
                             entry.line,
                             usize(1),
                             "Waywallen plugin translations do not support msgctxt"_str));
        if (entry.msgid_plural.is_some())
            return Err(error(ErrorCode::UnsupportedPlural,
                             entry.line,
                             usize(1),
                             "Waywallen plugin translations do not support plural entries"_str));
        if (entry.obsolete)
            obsolete.insert(entry.msgid.clone(), clone_entry(entry));
        else
            active.insert(entry.msgid.clone(), clone_entry(entry));
    }

    auto output = Document { Vec<Entry>::make() };
    auto header = build_header(project_id_version, canonical->as_str(), old_header);
    if (header.is_err()) return Err(rstd::move(header).unwrap_err_unchecked());
    output.entries.push(rstd::move(header).unwrap_unchecked());

    auto current = normalized_messages(messages);
    while (auto item = current.pop_first()) {
        auto source = rstd::move(item->template get<1>());
        auto prior  = active.remove(source.msgid.as_str());
        if (prior.is_none()) prior = obsolete.remove(source.msgid.as_str());
        auto translations = Vec<String>::make();
        translations.push(String::make());
        auto entry = Entry { Vec<String>::make(),
                             rstd::move(source.extracted_comments),
                             rstd::move(source.references),
                             Vec<String>::make(),
                             Vec<String>::make(),
                             None(),
                             rstd::move(source.msgid),
                             None(),
                             rstd::move(translations),
                             false,
                             usize {} };
        if (prior.is_some()) {
            entry.translator_comments = rstd::move(prior->translator_comments);
            entry.flags               = rstd::move(prior->flags);
            entry.previous            = rstd::move(prior->previous);
            entry.msgstrs             = rstd::move(prior->msgstrs);
            if (entry.msgstrs.is_empty()) entry.msgstrs.push(String::make());
        }
        output.entries.push(rstd::move(entry));
    }
    while (auto item = active.pop_first()) {
        auto entry     = rstd::move(item->template get<1>());
        entry.obsolete = true;
        obsolete.insert(entry.msgid.clone(), rstd::move(entry));
    }
    while (auto item = obsolete.pop_first())
        output.entries.push(rstd::move(item->template get<1>()));
    return Ok(render(output));
}

auto runtime_view(ref<str> locale, const Document& document) -> Result<RuntimeView> {
    auto validated = validate_header(locale, document);
    if (validated.is_err()) return Err(rstd::move(validated).unwrap_err_unchecked());
    auto translations = Vec<Translation>::make();
    for (const auto& entry : document.entries) {
        if (entry.obsolete || entry.is_header()) continue;
        if (entry.context.is_some())
            return Err(error(ErrorCode::UnsupportedContext,
                             entry.line,
                             usize(1),
                             "Waywallen plugin translations do not support msgctxt"_str));
        if (entry.msgid_plural.is_some())
            return Err(error(ErrorCode::UnsupportedPlural,
                             entry.line,
                             usize(1),
                             "Waywallen plugin translations do not support plural entries"_str));
        if (entry.is_fuzzy() || entry.msgstrs.is_empty() || entry.msgstrs[usize {}].is_empty())
            continue;
        translations.push(Translation { entry.msgid.clone(), entry.msgstrs[usize {}].clone() });
    }
    return Ok(RuntimeView { rstd::move(validated).unwrap_unchecked(), rstd::move(translations) });
}

auto check(ref<str> locale, const Document& document, slice<SourceMessage> messages)
    -> Result<empty> {
    auto validated = validate_header(locale, document);
    if (validated.is_err()) return Err(rstd::move(validated).unwrap_err_unchecked());
    auto expected = normalized_messages(messages);
    auto seen     = BTreeMap<String, empty>::make();
    for (const auto& entry : document.entries) {
        if (entry.is_header()) continue;
        if (entry.context.is_some())
            return Err(error(ErrorCode::UnsupportedContext,
                             entry.line,
                             usize(1),
                             "Waywallen plugin translations do not support msgctxt"_str));
        if (entry.msgid_plural.is_some())
            return Err(error(ErrorCode::UnsupportedPlural,
                             entry.line,
                             usize(1),
                             "Waywallen plugin translations do not support plural entries"_str));
        if (entry.obsolete)
            return Err(error(ErrorCode::ObsoleteTranslation,
                             entry.line,
                             usize(1),
                             "obsolete PO entries must be removed"_str));
        auto source = expected.get(entry.msgid.as_str());
        if (source.is_none())
            return Err(error(ErrorCode::SourceDrift,
                             entry.line,
                             usize(1),
                             "PO entry is not present in plugin sources"_str));
        if (entry.is_fuzzy())
            return Err(error(ErrorCode::FuzzyTranslation,
                             entry.line,
                             usize(1),
                             "fuzzy PO entry needs review"_str));
        if (entry.msgstrs.is_empty() || entry.msgstrs[usize {}].is_empty())
            return Err(error(ErrorCode::EmptyTranslation,
                             entry.line,
                             usize(1),
                             "PO entry has no translation"_str));
        if (! references_equal(entry.references, (*source)->references))
            return Err(error(ErrorCode::SourceDrift,
                             entry.line,
                             usize(1),
                             "PO source references are not synchronized"_str));
        seen.insert(entry.msgid.clone(), empty {});
    }
    if (seen.len() != expected.len())
        return Err(error(ErrorCode::SourceDrift,
                         usize(1),
                         usize(1),
                         "plugin source messages are missing from the PO file"_str));
    return Ok(empty {});
}

auto code_name(ErrorCode code) noexcept -> ref<str> {
    switch (code) {
    case ErrorCode::InvalidUtf8: return "invalid-utf8"_str;
    case ErrorCode::InvalidLocale: return "invalid-locale"_str;
    case ErrorCode::Syntax: return "po-syntax"_str;
    case ErrorCode::DuplicateEntry: return "po-duplicate-entry"_str;
    case ErrorCode::InvalidHeader: return "po-invalid-header"_str;
    case ErrorCode::LocaleMismatch: return "po-locale-mismatch"_str;
    case ErrorCode::UnsupportedContext: return "po-context-unsupported"_str;
    case ErrorCode::UnsupportedPlural: return "po-plural-unsupported"_str;
    case ErrorCode::EmptyTranslation: return "po-empty-translation"_str;
    case ErrorCode::FuzzyTranslation: return "po-fuzzy-translation"_str;
    case ErrorCode::ObsoleteTranslation: return "po-obsolete-translation"_str;
    case ErrorCode::SourceDrift: return "po-source-drift"_str;
    }
    rstd::unreachable();
}

} // namespace waywallen::i18n::po
