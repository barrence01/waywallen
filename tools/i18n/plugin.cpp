module;

#include <stdio.h>

module waywallen.i18n.plugin;

import luato.i18n;
import rstd;
import rstd.toml;
import waywallen.i18n.po;

namespace waywallen::i18n
{

using namespace rstd::literals;
using namespace rstd::prelude;
using ::alloc::collections::BTreeMap;
using ::alloc::string::String;
using ::alloc::vec::Vec;

struct Failure {
    ExitCode code;
    String   message;
};

template<typename T>
using ToolResult = Result<T, Failure>;

struct Project {
    rstd::path::PathBuf     root;
    String                  id;
    String                  version;
    String                  translation_directory;
    BTreeMap<String, empty> declared_files;
    rstd::toml::Value       manifest;
};

struct OwnedSource {
    String logical_path;
    String text;
};

auto with_newline(String message) -> String {
    if (! message.as_str().ends_with("\n"_str)) message.push_ascii('\n');
    return message;
}

auto failure(ExitCode code, String message) -> Failure {
    return Failure { code, with_newline(rstd::move(message)) };
}

auto path_text(ref<rstd::path::Path> path) -> String { return path.to_string_lossy(); }

template<typename Error>
auto io_failure(ref<rstd::path::Path> path, const Error& error) -> Failure {
    return failure(ExitCode::Io, rstd::format("{}: error[io]: {}", path, error));
}

void write_text(FILE* stream, ref<str> text) {
    if (text.is_empty()) return;
    (void)::fwrite(text.data(), 1, text.len().to_primitive(), stream);
}

auto emit(Failure value) -> ExitCode {
    write_text(stderr, value.message.as_str());
    return value.code;
}

void emit_diagnostic(const luato::i18n::Diagnostic& diagnostic) {
    auto text = rstd::format("{}:{}:{}: error[{}]: {}\n",
                             diagnostic.source.as_str(),
                             diagnostic.position.line,
                             diagnostic.position.column,
                             luato::i18n::code_name(diagnostic.code),
                             diagnostic.message.as_str());
    write_text(stderr, text.as_str());
    for (const auto& related : diagnostic.related) {
        auto line = rstd::format("  {}:{}:{}: note: {}\n",
                                 related.source.as_str(),
                                 related.position.line,
                                 related.position.column,
                                 related.message.as_str());
        write_text(stderr, line.as_str());
    }
}

void emit_po_error(ref<str> path, const po::Error& error) {
    auto text = rstd::format("{}:{}:{}: error[{}]: {}\n",
                             path,
                             error.line,
                             error.column,
                             po::code_name(error.code),
                             error.message.as_str());
    write_text(stderr, text.as_str());
}

auto join_path(ref<rstd::path::Path> base, ref<str> child) -> rstd::path::PathBuf {
    auto component = rstd::path::PathBuf::from(child);
    return rstd::path::PathBuf::from(base).join(component.as_path());
}

auto normalize_relative(ref<str> value, ref<str> owner) -> ToolResult<String> {
    auto input = rstd::path::PathBuf::from(value);
    if (! input.as_path().is_safe_relative()) {
        return Err(failure(ExitCode::Plugin,
                           rstd::format("{}: error[unsafe-plugin-path]: '{}' is not a safe "
                                        "relative path",
                                        owner,
                                        value)));
    }

    auto normalized = String::make();
    auto components = input.as_path().components();
    while (auto component = components.next()) {
        if (! component->is_normal()) continue;
        auto text = component->as_os_str().to_str();
        if (text.is_none()) {
            return Err(
                failure(ExitCode::Plugin,
                        rstd::format("{}: error[unsafe-plugin-path]: path is not UTF-8", owner)));
        }
        if (! normalized.is_empty()) normalized.push_ascii('/');
        normalized.push_str(*text);
    }
    if (normalized.is_empty()) {
        return Err(failure(ExitCode::Plugin,
                           rstd::format("{}: error[unsafe-plugin-path]: path is empty", owner)));
    }
    return Ok(rstd::move(normalized));
}

auto read_text_file(ref<rstd::path::Path> path) -> ToolResult<String> {
    auto contents = rstd::fs::read_to_string(path);
    if (contents.is_err()) return Err(io_failure(path, contents.unwrap_err()));
    return Ok(rstd::move(contents).unwrap_unchecked());
}

auto required_string(const rstd::toml::Value& table, ref<str> key, ref<rstd::path::Path> manifest,
                     ref<str> owner) -> ToolResult<String> {
    auto value = table.get(key);
    if (value.is_none() || ! (*value)->is_string()) {
        return Err(failure(
            ExitCode::Plugin,
            rstd::format(
                "{}: error[plugin-manifest]: '{}.{}' must be a string", manifest, owner, key)));
    }
    return Ok(String::make(*(*value)->as_str()));
}

auto parse_declared_files(ref<str> document, ref<rstd::path::Path> files_path)
    -> ToolResult<BTreeMap<String, empty>> {
    auto declared = BTreeMap<String, empty>::make();
    auto begin    = usize {};
    while (begin <= document.len()) {
        auto end = begin;
        while (end < document.len() && document.as_bytes()[end] != u8('\n')) ++end;
        auto line = document.get(begin, end).unwrap().trim_ascii();
        if (! line.is_empty()) {
            const bool relevant   = line.ends_with(".lua"_str) || line.ends_with(".po"_str);
            auto       normalized = normalize_relative(line, "files.txt"_str);
            if (normalized.is_err()) {
                if (relevant) return Err(rstd::move(normalized).unwrap_err_unchecked());
            } else {
                auto path = rstd::move(normalized).unwrap_unchecked();
                if (declared.insert(path.clone(), empty {}).is_some()) {
                    return Err(failure(ExitCode::Plugin,
                                       rstd::format("{}: error[plugin-files]: duplicate entry '{}'",
                                                    files_path,
                                                    path.as_str())));
                }
            }
        }
        if (end == document.len()) break;
        begin = end + usize(1);
    }
    return Ok(rstd::move(declared));
}

auto load_project(ref<rstd::path::Path> requested_root) -> ToolResult<Project> {
    auto canonical_root = rstd::fs::canonicalize(requested_root);
    if (canonical_root.is_err())
        return Err(io_failure(requested_root, canonical_root.unwrap_err()));
    auto root = rstd::move(canonical_root).unwrap_unchecked();

    auto root_metadata = rstd::fs::metadata(root.as_path());
    if (root_metadata.is_err()) return Err(io_failure(root.as_path(), root_metadata.unwrap_err()));
    if (! root_metadata->is_dir()) {
        return Err(
            failure(ExitCode::Plugin,
                    rstd::format("{}: error[plugin-root]: expected a directory", root.as_path())));
    }

    auto manifest_path = join_path(root.as_path(), "plugin.toml"_str);
    auto manifest_text = read_text_file(manifest_path.as_path());
    if (manifest_text.is_err()) return Err(rstd::move(manifest_text).unwrap_err_unchecked());
    auto parsed = rstd::toml::from_str(manifest_text->as_str());
    if (parsed.is_err()) {
        auto error = rstd::move(parsed).unwrap_err_unchecked();
        return Err(failure(ExitCode::Plugin,
                           rstd::format("{}:{}:{}: error[plugin-manifest]: {}",
                                        manifest_path.as_path(),
                                        error.line(),
                                        error.column(),
                                        error)));
    }
    auto document = rstd::move(parsed).unwrap_unchecked();
    auto plugin   = document.get("plugin"_str);
    if (plugin.is_none() || ! (*plugin)->is_table()) {
        return Err(failure(ExitCode::Plugin,
                           rstd::format("{}: error[plugin-manifest]: missing [plugin] table",
                                        manifest_path.as_path())));
    }
    auto id = required_string(**plugin, "id"_str, manifest_path.as_path(), "plugin"_str);
    if (id.is_err()) return Err(rstd::move(id).unwrap_err_unchecked());
    if (id->is_empty()) {
        return Err(failure(ExitCode::Plugin,
                           rstd::format("{}: error[plugin-manifest]: plugin.id cannot be empty",
                                        manifest_path.as_path())));
    }
    auto version = required_string(**plugin, "version"_str, manifest_path.as_path(), "plugin"_str);
    if (version.is_err()) return Err(rstd::move(version).unwrap_err_unchecked());
    if (version->is_empty()) {
        return Err(
            failure(ExitCode::Plugin,
                    rstd::format("{}: error[plugin-manifest]: plugin.version cannot be empty",
                                 manifest_path.as_path())));
    }

    auto i18n = (*plugin)->get("i18n"_str);
    if (i18n.is_none() || ! (*i18n)->is_table()) {
        return Err(failure(ExitCode::Plugin,
                           rstd::format("{}: error[plugin-manifest]: missing [plugin.i18n] table",
                                        manifest_path.as_path())));
    }
    auto directory =
        required_string(**i18n, "directory"_str, manifest_path.as_path(), "plugin.i18n"_str);
    if (directory.is_err()) return Err(rstd::move(directory).unwrap_err_unchecked());
    auto normalized_directory =
        normalize_relative(directory->as_str(), "plugin.i18n.directory"_str);
    if (normalized_directory.is_err()) {
        return Err(rstd::move(normalized_directory).unwrap_err_unchecked());
    }

    auto files_path = join_path(root.as_path(), "files.txt"_str);
    auto files_text = read_text_file(files_path.as_path());
    if (files_text.is_err()) return Err(rstd::move(files_text).unwrap_err_unchecked());
    auto declared = parse_declared_files(files_text->as_str(), files_path.as_path());
    if (declared.is_err()) return Err(rstd::move(declared).unwrap_err_unchecked());

    return Ok(Project { rstd::move(root),
                        rstd::move(id).unwrap_unchecked(),
                        rstd::move(version).unwrap_unchecked(),
                        rstd::move(normalized_directory).unwrap_unchecked(),
                        rstd::move(declared).unwrap_unchecked(),
                        rstd::move(document) });
}

auto validate_owned_file(const Project& project, ref<str> logical)
    -> ToolResult<rstd::path::PathBuf> {
    auto path      = join_path(project.root.as_path(), logical);
    auto canonical = rstd::fs::canonicalize(path.as_path());
    if (canonical.is_err()) return Err(io_failure(path.as_path(), canonical.unwrap_err()));
    auto owned = rstd::move(canonical).unwrap_unchecked();
    if (! owned.as_path().starts_with(project.root.as_path())) {
        return Err(failure(
            ExitCode::Plugin,
            rstd::format("{}: error[unsafe-plugin-path]: path escapes plugin root", logical)));
    }
    auto metadata = rstd::fs::metadata(owned.as_path());
    if (metadata.is_err()) return Err(io_failure(owned.as_path(), metadata.unwrap_err()));
    if (! metadata->is_file()) {
        return Err(
            failure(ExitCode::Plugin,
                    rstd::format("{}: error[plugin-files]: expected a regular file", logical)));
    }
    return Ok(rstd::move(owned));
}

auto load_sources(const Project& project) -> ToolResult<Vec<OwnedSource>> {
    auto sources        = Vec<OwnedSource>::make();
    auto canonical_seen = BTreeMap<String, empty>::make();
    for (auto item : project.declared_files.iter()) {
        auto [logical, unused] = item;
        (void)unused;
        if (! logical->as_str().ends_with(".lua"_str)) continue;
        auto actual = validate_owned_file(project, logical->as_str());
        if (actual.is_err()) return Err(rstd::move(actual).unwrap_err_unchecked());
        auto canonical_key = path_text(actual->as_path());
        if (canonical_seen.insert(rstd::move(canonical_key), empty {}).is_some()) {
            return Err(failure(
                ExitCode::Plugin,
                rstd::format("{}: error[plugin-files]: duplicate Lua file", logical->as_str())));
        }
        auto text = read_text_file(actual->as_path());
        if (text.is_err()) return Err(rstd::move(text).unwrap_err_unchecked());
        sources.push(OwnedSource { logical->clone(), rstd::move(text).unwrap_unchecked() });
    }
    return Ok(rstd::move(sources));
}

auto extract_sources(const Vec<OwnedSource>& owned) -> ToolResult<luato::i18n::Extraction> {
    auto sources = Vec<luato::i18n::SourceFile>::with_capacity(owned.len());
    for (const auto& source : owned) {
        sources.push(luato::i18n::SourceFile { rstd::parse::SourceId(source.logical_path.clone()),
                                               source.text.as_str().as_bytes() });
    }

    auto callee = Vec<String>::make();
    callee.push(String::make("tr"_str));
    auto options   = luato::i18n::ExtractionOptions { luato::i18n::CallSpec {
        rstd::move(callee),
        usize {},
        None(),
        String::make("TRANSLATORS:"_str),
        Some(String::make("tr"_str)),
    } };
    auto extracted = luato::i18n::extract(sources.as_slice(), options);
    if (extracted.is_err()) {
        emit_diagnostic(extracted.unwrap_err());
        return Err(failure(ExitCode::Source, String::make()));
    }
    return Ok(rstd::move(extracted).unwrap_unchecked());
}

auto manifest_message(const rstd::toml::Value& value, ref<str> owner)
    -> ToolResult<luato::i18n::Message> {
    if (! value.is_table()) {
        return Err(failure(ExitCode::Plugin,
                           rstd::format("plugin.toml: error[plugin-manifest]: '{}' must be an "
                                        "inline table with msgid",
                                        owner)));
    }
    auto manifest_path = rstd::path::PathBuf::from("plugin.toml"_str);
    auto msgid         = required_string(value, "msgid"_str, manifest_path.as_path(), owner);
    if (msgid.is_err()) return Err(rstd::move(msgid).unwrap_err_unchecked());
    if (msgid->is_empty()) {
        return Err(failure(ExitCode::Plugin,
                           rstd::format("plugin.toml: error[plugin-manifest]: '{}.msgid' cannot be "
                                        "empty",
                                        owner)));
    }

    auto occurrences = Vec<luato::i18n::Occurrence>::make();
    occurrences.push(luato::i18n::Occurrence {
        rstd::parse::SourceId("plugin.toml"_str),
        rstd::parse::Span {},
        rstd::parse::Span {},
        rstd::parse::SourcePosition { usize {}, usize {} },
        None(),
    });
    return Ok(luato::i18n::Message {
        rstd::move(msgid).unwrap_unchecked(), rstd::move(occurrences), Vec<String>::make() });
}

auto extract_manifest(const Project& project) -> ToolResult<luato::i18n::Extraction> {
    auto messages  = Vec<luato::i18n::Message>::make();
    auto renderers = project.manifest.get("renderers"_str);
    if (renderers.is_none()) {
        return Ok(
            luato::i18n::Extraction { rstd::move(messages), Vec<luato::i18n::Diagnostic>::make() });
    }
    auto renderer_table = (*renderers)->as_table();
    if (renderer_table.is_none()) {
        return Err(
            failure(ExitCode::Plugin,
                    String::make("plugin.toml: error[plugin-manifest]: 'renderers' must be a "
                                 "table"_str)));
    }
    for (auto renderer_item : (**renderer_table).iter()) {
        auto [renderer_name, renderer] = renderer_item;
        auto settings                  = renderer->get("settings"_str);
        if (settings.is_none()) continue;
        auto setting_table = (*settings)->as_table();
        if (setting_table.is_none()) {
            return Err(failure(
                ExitCode::Plugin,
                rstd::format("plugin.toml: error[plugin-manifest]: 'renderers.{}.settings' must be "
                             "a table",
                             renderer_name->as_str())));
        }
        for (auto setting_item : (**setting_table).iter()) {
            auto [setting_name, setting] = setting_item;
            static constexpr auto text_fields =
                rstd::array<ref<str>, 3> { "label"_str, "description"_str, "group_label"_str };
            for (auto field : text_fields) {
                auto declared = setting->get(field);
                if (declared.is_none()) continue;
                auto owner   = rstd::format("renderers.{}.settings.{}.{}",
                                            renderer_name->as_str(),
                                            setting_name->as_str(),
                                            field);
                auto message = manifest_message(**declared, owner.as_str());
                if (message.is_err()) return Err(rstd::move(message).unwrap_err_unchecked());
                messages.push(rstd::move(message).unwrap_unchecked());
            }
        }
    }
    return Ok(
        luato::i18n::Extraction { rstd::move(messages), Vec<luato::i18n::Diagnostic>::make() });
}

auto merge_project_extractions(luato::i18n::Extraction lua, luato::i18n::Extraction manifest)
    -> luato::i18n::Extraction {
    auto extractions = Vec<luato::i18n::Extraction>::make();
    extractions.push(rstd::move(lua));
    extractions.push(rstd::move(manifest));
    return luato::i18n::merge_extractions(rstd::move(extractions));
}

auto translation_logical_path(const Project& project, ref<str> locale) -> ToolResult<String> {
    auto candidate = rstd::format("{}/{}.po", project.translation_directory.as_str(), locale);
    return normalize_relative(candidate.as_str(), "translation"_str);
}

auto canonical_locale(ref<str> locale) -> ToolResult<String> {
    auto canonical = po::canonicalize_locale(locale);
    if (canonical.is_err()) {
        return Err(failure(ExitCode::Usage,
                           rstd::format("error: invalid locale '{}': {}",
                                        locale,
                                        canonical.unwrap_err().message.as_str())));
    }
    return Ok(rstd::move(canonical).unwrap_unchecked());
}

auto translation_directory_path(const Project& project, bool create)
    -> ToolResult<rstd::path::PathBuf> {
    auto path   = join_path(project.root.as_path(), project.translation_directory.as_str());
    auto exists = rstd::fs::exists(path.as_path());
    if (exists.is_err()) return Err(io_failure(path.as_path(), exists.unwrap_err()));
    if (! *exists) {
        if (! create) {
            return Err(failure(ExitCode::Io,
                               rstd::format("{}: error[io]: translation directory does not exist",
                                            path.as_path())));
        }
        auto created = rstd::fs::create_dir_all(path.as_path());
        if (created.is_err()) return Err(io_failure(path.as_path(), created.unwrap_err()));
    }
    auto canonical = rstd::fs::canonicalize(path.as_path());
    if (canonical.is_err()) return Err(io_failure(path.as_path(), canonical.unwrap_err()));
    auto owned = rstd::move(canonical).unwrap_unchecked();
    if (! owned.as_path().starts_with(project.root.as_path())) {
        return Err(
            failure(ExitCode::Plugin,
                    rstd::format("{}: error[unsafe-plugin-path]: translation directory escapes "
                                 "plugin root",
                                 project.translation_directory.as_str())));
    }
    auto metadata = rstd::fs::metadata(owned.as_path());
    if (metadata.is_err()) return Err(io_failure(owned.as_path(), metadata.unwrap_err()));
    if (! metadata->is_dir()) {
        return Err(
            failure(ExitCode::Plugin,
                    rstd::format("{}: error[plugin-manifest]: translation path is not a directory",
                                 project.translation_directory.as_str())));
    }
    return Ok(rstd::move(owned));
}

auto read_existing_translation(const Project& project, ref<str> logical,
                               ref<rstd::path::Path> directory) -> ToolResult<Option<String>> {
    auto logical_path = rstd::path::PathBuf::from(logical);
    auto filename     = logical_path.as_path().file_name();
    if (filename.is_none()) {
        return Err(failure(
            ExitCode::Plugin,
            rstd::format("{}: error[unsafe-plugin-path]: invalid translation path", logical)));
    }
    auto path   = rstd::path::PathBuf::from(directory).join(ref<rstd::path::Path>(*filename));
    auto exists = rstd::fs::exists(path.as_path());
    if (exists.is_err()) return Err(io_failure(path.as_path(), exists.unwrap_err()));
    if (! *exists) return Ok(None());
    auto canonical = validate_owned_file(project, logical);
    if (canonical.is_err()) return Err(rstd::move(canonical).unwrap_err_unchecked());
    auto document = read_text_file(canonical->as_path());
    if (document.is_err()) return Err(rstd::move(document).unwrap_err_unchecked());
    return Ok(Some(rstd::move(document).unwrap_unchecked()));
}

auto po_messages(const luato::i18n::Extraction& extraction) -> Vec<po::SourceMessage> {
    auto output = Vec<po::SourceMessage>::with_capacity(extraction.messages.len());
    for (const auto& message : extraction.messages) {
        auto references = BTreeMap<String, empty>::make();
        for (const auto& occurrence : message.occurrences) {
            auto reference =
                occurrence.position.line == usize {}
                    ? String::make(occurrence.source.as_str())
                    : rstd::format("{}:{}", occurrence.source.as_str(), occurrence.position.line);
            references.insert(rstd::move(reference), empty {});
        }
        auto ordered_references = Vec<String>::with_capacity(references.len());
        while (auto reference = references.pop_first())
            ordered_references.push(rstd::move(reference->template get<0>()));
        auto comments = Vec<String>::with_capacity(message.translator_notes.len());
        for (const auto& note : message.translator_notes) comments.push(note.clone());
        output.push(po::SourceMessage {
            message.msgid.clone(), rstd::move(ordered_references), rstd::move(comments) });
    }
    return output;
}

auto update(const Project& project, const Vec<String>& locales,
            const luato::i18n::Extraction& extraction) -> ExitCode {
    if (locales.len() != usize(1)) {
        return emit(failure(ExitCode::Usage,
                            String::make("error: update requires exactly one --locale"_str)));
    }
    auto locale = canonical_locale(locales[usize()].as_str());
    if (locale.is_err()) return emit(rstd::move(locale).unwrap_err_unchecked());
    auto logical = translation_logical_path(project, locale->as_str());
    if (logical.is_err()) return emit(rstd::move(logical).unwrap_err_unchecked());
    if (! project.declared_files.contains_key(logical->as_str())) {
        return emit(failure(ExitCode::Plugin,
                            rstd::format("{}: error[plugin-files]: translation must be declared in "
                                         "files.txt",
                                         logical->as_str())));
    }

    auto directory = translation_directory_path(project, true);
    if (directory.is_err()) return emit(rstd::move(directory).unwrap_err_unchecked());
    auto existing = read_existing_translation(project, logical->as_str(), directory->as_path());
    if (existing.is_err()) return emit(rstd::move(existing).unwrap_err_unchecked());
    auto existing_ref = Option<ref<str>> {};
    if (existing->is_some()) existing_ref = Some(existing->as_ref().unwrap().as_str());
    auto messages           = po_messages(extraction);
    auto project_id_version = rstd::format("{} {}", project.id.as_str(), project.version.as_str());
    auto rendered           = po::update(
        project_id_version.as_str(), locale->as_str(), existing_ref, messages.as_slice());
    if (rendered.is_err()) {
        emit_po_error(logical->as_str(), rendered.unwrap_err());
        return ExitCode::Translation;
    }

    auto logical_path = rstd::path::PathBuf::from(logical->as_str());
    auto filename     = logical_path.as_path().file_name().unwrap();
    auto output_path  = directory->join(ref<rstd::path::Path>(filename));
    auto written =
        rstd::fs::write_atomic_if_changed(output_path.as_path(), rendered->as_str().as_bytes());
    if (written.is_err()) return emit(io_failure(output_path.as_path(), written.unwrap_err()));
    auto summary = rstd::format("{}: updated {}\n", project.id.as_str(), logical->as_str());
    write_text(stdout, summary.as_str());
    return ExitCode::Success;
}

auto requested_translations(const Project& project, const Vec<String>& locales,
                            ref<rstd::path::Path> directory)
    -> ToolResult<BTreeMap<String, String>> {
    auto translations = BTreeMap<String, String>::make();
    if (! locales.is_empty()) {
        for (const auto& locale : locales) {
            auto canonical = canonical_locale(locale.as_str());
            if (canonical.is_err()) return Err(rstd::move(canonical).unwrap_err_unchecked());
            auto logical = translation_logical_path(project, canonical->as_str());
            if (logical.is_err()) return Err(rstd::move(logical).unwrap_err_unchecked());
            if (! project.declared_files.contains_key(logical->as_str())) {
                return Err(
                    failure(ExitCode::Plugin,
                            rstd::format("{}: error[plugin-files]: translation is not declared "
                                         "in files.txt",
                                         logical->as_str())));
            }
            if (translations
                    .insert(rstd::move(canonical).unwrap_unchecked(),
                            rstd::move(logical).unwrap_unchecked())
                    .is_some()) {
                return Err(
                    failure(ExitCode::Usage,
                            rstd::format("error: duplicate --locale '{}'", locale.as_str())));
            }
        }
        return Ok(rstd::move(translations));
    }

    auto entries = rstd::fs::read_dir(directory);
    if (entries.is_err()) return Err(io_failure(directory, entries.unwrap_err()));
    auto reader = rstd::move(entries).unwrap_unchecked();
    while (auto next = reader.next()) {
        auto entry_result = rstd::move(*next);
        if (entry_result.is_err()) return Err(io_failure(directory, entry_result.unwrap_err()));
        auto entry = rstd::move(entry_result).unwrap_unchecked();
        auto type  = entry.file_type();
        if (type.is_err()) return Err(io_failure(entry.path().as_path(), type.unwrap_err()));
        if (! type->is_file() && ! type->is_symlink()) continue;
        auto file_name = entry.file_name();
        auto name      = file_name.as_os_str().to_str();
        if (name.is_none() || ! name->ends_with(".po"_str)) continue;
        auto locale    = name->strip_suffix(".po"_str).unwrap();
        auto canonical = canonical_locale(locale);
        if (canonical.is_err() || canonical->as_str() != locale) {
            return Err(failure(ExitCode::Plugin,
                               rstd::format("{}: error[plugin-files]: translation filename is not "
                                            "a canonical BCP 47 locale",
                                            *name)));
        }
        auto logical = translation_logical_path(project, locale);
        if (logical.is_err()) return Err(rstd::move(logical).unwrap_err_unchecked());
        if (! project.declared_files.contains_key(logical->as_str())) {
            return Err(
                failure(ExitCode::Plugin,
                        rstd::format("{}: error[plugin-files]: translation is not declared in "
                                     "files.txt",
                                     logical->as_str())));
        }
        translations.insert(String::make(locale), rstd::move(logical).unwrap_unchecked());
    }
    if (translations.is_empty()) {
        return Err(failure(ExitCode::Plugin,
                           rstd::format("{}: error[plugin-files]: no translations to check",
                                        project.translation_directory.as_str())));
    }
    return Ok(rstd::move(translations));
}

auto check(const Project& project, const Vec<String>& locales,
           const luato::i18n::Extraction& extraction) -> ExitCode {
    auto directory = translation_directory_path(project, false);
    if (directory.is_err()) return emit(rstd::move(directory).unwrap_err_unchecked());
    auto translations = requested_translations(project, locales, directory->as_path());
    if (translations.is_err()) return emit(rstd::move(translations).unwrap_err_unchecked());

    auto messages = po_messages(extraction);
    for (auto item : translations->iter()) {
        auto [locale, logical] = item;
        auto document = read_existing_translation(project, logical->as_str(), directory->as_path());
        if (document.is_err()) return emit(rstd::move(document).unwrap_err_unchecked());
        if (document->is_none()) {
            return emit(
                failure(ExitCode::Translation,
                        rstd::format("{}: error[translation-missing]: translation does not exist",
                                     logical->as_str())));
        }
        auto text   = document->as_ref().unwrap().as_str();
        auto parsed = po::parse(text);
        if (parsed.is_err()) {
            emit_po_error(logical->as_str(), parsed.unwrap_err());
            return ExitCode::Translation;
        }
        auto synchronized = po::check(locale->as_str(), *parsed, messages.as_slice());
        if (synchronized.is_err()) {
            emit_po_error(logical->as_str(), synchronized.unwrap_err());
            return ExitCode::Translation;
        }
    }

    auto summary = rstd::format(
        "{}: {} translation(s) synchronized\n", project.id.as_str(), translations->len());
    write_text(stdout, summary.as_str());
    return ExitCode::Success;
}

auto execute(const Request& request) -> ExitCode {
    if (request.mode == Mode::Update && request.locales.len() != usize(1)) {
        return emit(failure(ExitCode::Usage,
                            String::make("error: update requires exactly one --locale"_str)));
    }
    auto project = load_project(request.plugin.as_path());
    if (project.is_err()) return emit(rstd::move(project).unwrap_err_unchecked());
    auto sources = load_sources(*project);
    if (sources.is_err()) return emit(rstd::move(sources).unwrap_err_unchecked());
    auto extraction = extract_sources(*sources);
    if (extraction.is_err()) return extraction.unwrap_err().code;
    auto manifest = extract_manifest(*project);
    if (manifest.is_err()) return emit(rstd::move(manifest).unwrap_err_unchecked());
    auto merged = merge_project_extractions(rstd::move(extraction).unwrap_unchecked(),
                                            rstd::move(manifest).unwrap_unchecked());

    if (request.mode == Mode::Update) return update(*project, request.locales, merged);
    return check(*project, request.locales, merged);
}

} // namespace waywallen::i18n
