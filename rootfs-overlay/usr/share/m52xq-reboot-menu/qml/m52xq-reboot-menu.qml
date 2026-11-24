// M52XQ (Sailfish): reboot menu — reboot, reboot into TWRP, power off, cancel. Port of the pmOS/Phosh m52xq-reboot-menu
// (GTK4/libadwaita) to Silica. The action is written to /run/m52xq-power/request; the root unit m52xq-power-request.path
// runs it ("systemctl reboot recovery" = TWRP: restart_reason 0x77665502 via syscon-reboot-mode). Title in English,
// buttons in the session language.
import QtQuick 2.0
import Sailfish.Silica 1.0

ApplicationWindow {
    id: app
    readonly property bool ru: Qt.locale().name.indexOf("ru") === 0
    function request(action) {
        var x = new XMLHttpRequest()
        // quit only after the write finished: file PUT is asynchronous, Qt.quit() right after send() left an empty request file
        x.onreadystatechange = function() { if (x.readyState === XMLHttpRequest.DONE) Qt.quit() }
        x.open("PUT", "file:///run/m52xq-power/request")
        x.send(action)
    }
    cover: null
    allowedOrientations: Orientation.All
    initialPage: Component {
        Page {
            allowedOrientations: Orientation.All
            Column {
                width: parent.width
                spacing: Theme.paddingLarge
                anchors.verticalCenter: parent.verticalCenter
                PageHeader { title: "Reboot menu" }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 4 * Theme.horizontalPageMargin
                    text: app.ru ? "Перезагрузка" : "Reboot"
                    onClicked: app.request("reboot")
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 4 * Theme.horizontalPageMargin
                    text: app.ru ? "Перезагрузка в TWRP (recovery)" : "Reboot into TWRP (recovery)"
                    onClicked: app.request("recovery")
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 4 * Theme.horizontalPageMargin
                    text: app.ru ? "Выключение" : "Power off"
                    onClicked: app.request("poweroff")
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 4 * Theme.horizontalPageMargin
                    text: app.ru ? "Отмена" : "Cancel"
                    onClicked: Qt.quit()
                }
            }
        }
    }
}
