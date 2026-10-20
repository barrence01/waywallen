pub(super) fn find_ts_context<'a>(xml: &'a str, wanted: &str) -> Result<Option<&'a str>, String> {
    let mut rest = xml;
    while let Some(start) = rest.find("<context>") {
        let body_start = start + "<context>".len();
        let after_start = &rest[body_start..];
        let end = after_start
            .find("</context>")
            .ok_or_else(|| "unterminated <context>".to_string())?;
        let body = &after_start[..end];

        let (_, raw_name) =
            xml_tag(body, "name").ok_or_else(|| "<context> missing <name>".to_string())?;
        let name = decode_xml_text(raw_name.trim())?;
        if name == wanted {
            return Ok(Some(body));
        }

        rest = &after_start[end + "</context>".len()..];
    }
    Ok(None)
}

pub(super) fn parse_ts_messages(context: &str) -> Result<Vec<(String, Option<String>)>, String> {
    let mut messages = Vec::new();
    let mut rest = context;

    while let Some(start) = rest.find("<message") {
        let open_end = rest[start..]
            .find('>')
            .map(|end| start + end)
            .ok_or_else(|| "unterminated <message> start tag".to_string())?;
        let body_start = open_end + 1;
        let after_start = &rest[body_start..];
        let end = after_start
            .find("</message>")
            .ok_or_else(|| "unterminated <message>".to_string())?;
        let message = &after_start[..end];

        let (_, raw_source) =
            xml_tag(message, "source").ok_or_else(|| "<message> missing <source>".to_string())?;
        let (translation_attrs, raw_translation) = xml_tag(message, "translation")
            .ok_or_else(|| "<message> missing <translation>".to_string())?;

        if raw_source.contains('<') || raw_translation.contains('<') {
            return Err("nested XML is not supported in TrayMenu messages".to_string());
        }

        let source = decode_xml_text(raw_source.trim())?;
        let unfinished = translation_attrs.contains("type=\"unfinished\"")
            || translation_attrs.contains("type=\"vanished\"")
            || translation_attrs.contains("type=\"obsolete\"");
        let translation = if unfinished || raw_translation.trim().is_empty() {
            None
        } else {
            Some(decode_xml_text(raw_translation.trim())?)
        };

        messages.push((source, translation));
        rest = &after_start[end + "</message>".len()..];
    }

    if messages.is_empty() {
        return Err("TrayMenu context contains no messages".to_string());
    }
    Ok(messages)
}

fn xml_tag<'a>(block: &'a str, tag: &str) -> Option<(&'a str, &'a str)> {
    let open = format!("<{tag}");
    let start = block.find(&open)?;
    let attrs_start = start + open.len();
    let open_end = attrs_start + block[attrs_start..].find('>')?;
    let attrs = &block[attrs_start..open_end];
    if let Some(attrs) = attrs.strip_suffix('/') {
        return Some((attrs, ""));
    }

    let body_start = open_end + 1;
    let close = format!("</{tag}>");
    let body_end = body_start + block[body_start..].find(&close)?;
    Some((attrs, &block[body_start..body_end]))
}

fn decode_xml_text(text: &str) -> Result<String, String> {
    let mut out = String::with_capacity(text.len());
    let mut rest = text;

    while let Some(start) = rest.find('&') {
        out.push_str(&rest[..start]);
        let entity_start = start + 1;
        let after_amp = &rest[entity_start..];
        let end = after_amp
            .find(';')
            .ok_or_else(|| "unterminated XML entity".to_string())?;
        let entity = &after_amp[..end];

        let ch = match entity {
            "amp" => '&',
            "lt" => '<',
            "gt" => '>',
            "quot" => '"',
            "apos" => '\'',
            _ => {
                let code = if let Some(hex) = entity
                    .strip_prefix("#x")
                    .or_else(|| entity.strip_prefix("#X"))
                {
                    u32::from_str_radix(hex, 16)
                        .map_err(|_| format!("invalid XML entity &{entity};"))?
                } else if let Some(decimal) = entity.strip_prefix('#') {
                    decimal
                        .parse::<u32>()
                        .map_err(|_| format!("invalid XML entity &{entity};"))?
                } else {
                    return Err(format!("unsupported XML entity &{entity};"));
                };
                char::from_u32(code).ok_or_else(|| format!("invalid Unicode scalar U+{code:X}"))?
            }
        };
        out.push(ch);
        rest = &after_amp[end + 1..];
    }

    out.push_str(rest);
    Ok(out)
}
