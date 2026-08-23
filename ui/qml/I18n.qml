pragma Singleton
import QtQml
import waywallen.ui

QtObject {
    readonly property var revision: PluginTranslations.revision

    function tr(value) {
        const dependency = revision;
        return PluginTranslations.translate(value);
    }
}
