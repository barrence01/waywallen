import QtQuick
import QtQuick.Window
import QtTest
import "../qml/component" as UI

TestCase {
    id: testCase
    name: "WindowState"

    Component {
        id: windowComponent

        Window {
            id: win
            property UI.WindowState savedState: UI.WindowState {
                window: win
            }
        }
    }

    function initTestCase() {
        Qt.application.organization = "waywallen-tests";
        Qt.application.name = "window-state";
    }

    function createWindow(maximized) {
        const win = createTemporaryObject(windowComponent, testCase);
        verify(win !== null);
        win.savedState.settings.width = 640;
        win.savedState.settings.height = 480;
        win.savedState.settings.maximized = maximized;
        return win;
    }

    function test_restoreNormal() {
        const win = createWindow(false);
        win.savedState.restore();
        compare(win.visibility, Window.Windowed);
        compare(win.width, 640);
        compare(win.height, 480);
        win.width = 700;
        win.height = 500;
        tryCompare(win.savedState.settings, "width", 700);
        tryCompare(win.savedState.settings, "height", 500);
    }

    function test_restoreMaximized() {
        const win = createWindow(true);
        win.savedState.restore();
        compare(win.visibility, Window.Maximized);
        win.width = 1920;
        win.height = 1080;
        wait(0);
        compare(win.savedState.settings.width, 640);
        compare(win.savedState.settings.height, 480);
        compare(win.savedState.settings.maximized, true);
    }

    function test_restoreNormalAfterMaximized() {
        const win = createWindow(false);
        win.savedState.restore();
        win.showMaximized();
        win.width = 1920;
        win.height = 1080;
        wait(0);
        compare(win.savedState.settings.maximized, true);
        win.showNormal();
        compare(win.savedState.settings.maximized, false);
        // The state notification can precede the compositor's resize.
        compare(win.savedState.settings.width, 640);
        compare(win.savedState.settings.height, 480);
        win.width = 700;
        win.height = 500;
        tryCompare(win.savedState.settings, "width", 700);
        tryCompare(win.savedState.settings, "height", 500);
    }

    function test_maximizedGeometryBeforeState() {
        const win = createWindow(false);
        win.savedState.restore();
        wait(0);
        win.width = 1920;
        win.height = 1080;
        win.showMaximized();
        wait(0);
        compare(win.savedState.settings.width, 640);
        compare(win.savedState.settings.height, 480);
        compare(win.savedState.settings.maximized, true);
    }

    function test_closeBeforeDeferredSave() {
        const win = createWindow(false);
        win.savedState.restore();
        win.width = 700;
        win.height = 500;
        win.close();
        compare(win.savedState.settings.width, 700);
        compare(win.savedState.settings.height, 500);
        wait(0);
        compare(win.savedState.settings.maximized, false);
    }

    function test_transientVisibility_data() {
        return [
            {
                tag: "hidden-normal",
                maximized: false,
                visibility: Window.Hidden
            },
            {
                tag: "hidden-maximized",
                maximized: true,
                visibility: Window.Hidden
            },
            {
                tag: "minimized-normal",
                maximized: false,
                visibility: Window.Minimized
            },
            {
                tag: "minimized-maximized",
                maximized: true,
                visibility: Window.Minimized
            },
            {
                tag: "fullscreen-normal",
                maximized: false,
                visibility: Window.FullScreen
            },
            {
                tag: "fullscreen-maximized",
                maximized: true,
                visibility: Window.FullScreen
            }
        ];
    }

    function test_transientVisibility(data) {
        const win = createWindow(data.maximized);
        win.savedState.restore();
        win.visibility = data.visibility;
        win.width = 1920;
        win.height = 1080;
        wait(0);
        compare(win.savedState.settings.maximized, data.maximized);
        compare(win.savedState.settings.width, 640);
        compare(win.savedState.settings.height, 480);
    }

    function test_persistedRestart() {
        const win = createWindow(false);
        win.savedState.restore();
        win.width = 700;
        win.height = 500;
        tryCompare(win.savedState.settings, "width", 700);
        tryCompare(win.savedState.settings, "height", 500);
        win.showMaximized();
        win.width = 1920;
        win.height = 1080;
        tryVerify(() => win.savedState.settings.value("width") === 700 && win.savedState.settings.value("height") === 500 && win.savedState.settings.value("maximized") === true);
        win.savedState.settings.sync();

        const next = createTemporaryObject(windowComponent, testCase);
        verify(next !== null);
        compare(next.savedState.settings.width, 700);
        compare(next.savedState.settings.height, 500);
        next.savedState.restore();
        compare(next.visibility, Window.Maximized);
    }

    function test_showExistingMaximized() {
        const win = createWindow(true);
        win.savedState.restore();
        win.hide();
        win.visible = true;
        compare(win.visibility, Window.Maximized);
        compare(win.savedState.settings.maximized, true);
    }

    function test_restoreOnce() {
        const win = createWindow(false);
        win.savedState.restore();
        win.hide();
        win.savedState.restore();
        compare(win.visibility, Window.Hidden);
    }

    function test_invalidSize() {
        const win = createWindow(false);
        win.savedState.settings.width = 0;
        win.savedState.settings.height = -1;
        win.savedState.restore();
        compare(win.width, 948);
        compare(win.height, 632);
    }
}
