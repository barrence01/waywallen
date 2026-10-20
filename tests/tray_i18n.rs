#[path = "../tools/tray_i18n.rs"]
mod tray_i18n;

use tray_i18n::{find_ts_context, parse_ts_messages};

#[test]
fn empty_and_unfinished_translations_fall_back_to_source() {
    for translation in [
        "<translation/>",
        "<translation />",
        "<translation type=\"unfinished\"/>",
        "<translation type=\"unfinished\" />",
        "<translation></translation>",
        "<translation type=\"unfinished\">Weiter</translation>",
    ] {
        let xml = format!(
            "<context><name>TrayMenu</name>\
             <message><source>Next</source>{translation}</message>\
             <message><source>Previous</source><translation>Zurück</translation></message>\
             </context>"
        );
        let context = find_ts_context(&xml, "TrayMenu").unwrap().unwrap();
        assert_eq!(
            parse_ts_messages(context).unwrap(),
            vec![
                ("Next".to_string(), None),
                ("Previous".to_string(), Some("Zurück".to_string())),
            ],
            "{translation}"
        );
    }
}

#[test]
fn missing_or_unclosed_translation_is_rejected() {
    for translation in ["", "<translation>Weiter"] {
        let message = format!("<message><source>Next</source>{translation}</message>");
        assert!(parse_ts_messages(&message).is_err());
    }
}
