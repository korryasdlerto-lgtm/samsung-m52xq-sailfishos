// M52XQ (Sailfish): SIM cards manager. Port of the pmOS/Phosh m52xq-sim-manager (GTK4/libadwaita) to Silica:
// both SIM cards / only SIM 1 / only SIM 2, mobile data on/off, the SIM of the mobile data, SIM cards and modem radio
// off/on, Cancel. A request is written to /run/m52xq-sim/request, the root unit m52xq-sim-request.path runs
// /usr/local/sbin/m52xq-sim-mode, which writes the state to /run/m52xq-sim/state (read here every 2 s).
// Title in English, the rest in the session language.
import QtQuick 2.0
import Sailfish.Silica 1.0

ApplicationWindow {
    id: app
    readonly property bool ru: Qt.locale().name.indexOf("ru") === 0
    function tr(r, e) { return ru ? r : e }

    property string mode: "dsds"
    property string cards: ""
    property bool flight: false
    property bool conn: false
    property bool dataOn: false
    property bool dataUp: false
    property string dataSim: "1"
    property bool busy: true
    property bool updating: false
    property string message: ""

    function request(action, msg) {
        var x = new XMLHttpRequest()
        x.open("PUT", "file:///run/m52xq-sim/request")
        x.send(action)
        busy = true
        message = msg
    }
    function readState() {
        var x = new XMLHttpRequest()
        x.onreadystatechange = function() {
            if (x.readyState !== XMLHttpRequest.DONE || !x.responseText)
                return
            try {
                var s = JSON.parse(x.responseText)
                updating = true
                mode = s.mode; cards = s.cards; flight = s.flight === 1; conn = s.conn === 1
                dataOn = s.data === 1; dataUp = s.up === 1; busy = s.busy === 1
                if (s.datasim === "1" || s.datasim === "2") dataSim = s.datasim
                updating = false
            } catch (e) {}
        }
        x.open("GET", "file:///run/m52xq-sim/state")
        x.send()
    }
    // with one SIM card inserted only the mode of that card can be chosen (as on pmOS)
    function allowed(key) {
        if (flight) return false
        if (cards === "1") return key === "sim1"
        if (cards === "2") return key === "sim2"
        return true
    }

    // the state is written by the root script: read it every 2 s and ask for a new one every 5 s (changes made in the Sailfish
    // settings or in the top menu appear here)
    function refresh() {
        var x = new XMLHttpRequest()
        x.open("PUT", "file:///run/m52xq-sim/request")
        x.send("refresh")
    }
    Timer { interval: 2000; running: true; repeat: true; triggeredOnStart: true; onTriggered: app.readState() }
    Timer { interval: 5000; running: Qt.application.active; repeat: true; onTriggered: if (!app.busy) app.refresh() }
    Component.onCompleted: request("status", "")

    cover: null
    allowedOrientations: Orientation.All
    initialPage: Component {
        Page {
            allowedOrientations: Orientation.All
            SilicaFlickable {
                anchors.fill: parent
                contentHeight: col.height + Theme.paddingLarge
                Column {
                    id: col
                    width: parent.width
                    PageHeader { title: "SIM cards manager" }

                    SectionHeader { text: app.tr("Активные SIM-карты", "Active SIM cards") }
                    Repeater {
                        model: [["dsds", app.tr("Обе SIM-карты", "Both SIM cards")],
                                ["sim1", app.tr("Только SIM 1", "Only SIM 1")],
                                ["sim2", app.tr("Только SIM 2", "Only SIM 2")]]
                        TextSwitch {
                            text: modelData[1]
                            automaticCheck: false
                            checked: app.mode === modelData[0]
                            enabled: !app.busy && app.allowed(modelData[0])
                            onClicked: if (app.mode !== modelData[0])
                                app.request(modelData[0], app.tr("Готово, SIM-карты регистрируются в сети (~30 с)",
                                                                 "Done, the SIM cards register (~30 s)"))
                        }
                    }
                    Label {
                        x: Theme.horizontalPageMargin; width: parent.width - 2 * x; wrapMode: Text.Wrap
                        visible: app.cards === "1" || app.cards === "2"
                        color: Theme.secondaryHighlightColor; font.pixelSize: Theme.fontSizeSmall
                        text: app.tr("Вставлена одна SIM-карта: выбор режима недоступен", "One SIM card inserted: mode choice is off")
                    }

                    SectionHeader { text: app.tr("Мобильные данные", "Mobile data") }
                    TextSwitch {
                        text: app.tr("Мобильные данные включены", "Mobile data on")
                        automaticCheck: false
                        checked: app.dataOn
                        enabled: !app.busy && app.conn && !app.flight
                        onClicked: app.request(app.dataOn ? "data-off" : "data-on", app.tr("Мобильные данные", "Mobile data"))
                    }
                    ComboBox {
                        label: app.tr("SIM мобильных данных", "SIM of the mobile data")
                        currentIndex: app.dataSim === "2" ? 1 : 0
                        enabled: !app.busy && !app.flight && app.mode === "dsds" && app.cards !== "1" && app.cards !== "2"
                        value: app.dataSim === "2" ? "SIM 2" : "SIM 1"
                        menu: ContextMenu {
                            MenuItem {
                                text: "SIM 1"
                                onClicked: if (app.dataSim !== "1")
                                    app.request("data-sim1", app.tr("Данные через SIM 1", "Mobile data through SIM 1"))
                            }
                            MenuItem {
                                text: "SIM 2"
                                onClicked: if (app.dataSim !== "2")
                                    app.request("data-sim2", app.tr("Данные через SIM 2", "Mobile data through SIM 2"))
                            }
                        }
                        description: app.dataOn ? (app.dataUp ? app.tr("Соединение установлено", "Connected")
                                                              : app.tr("Включено, соединение ждёт (например, работает Wi-Fi)",
                                                                       "On, waiting (Wi-Fi is in use, for example)"))
                                                : ""
                    }
                    Label {
                        x: Theme.horizontalPageMargin; width: parent.width - 2 * x; wrapMode: Text.Wrap
                        visible: !app.conn && !app.flight && !app.busy
                        color: Theme.secondaryHighlightColor; font.pixelSize: Theme.fontSizeSmall
                        text: app.tr("Нет мобильного подключения в connman", "No mobile connection in connman")
                    }

                    SectionHeader { text: app.tr("SIM-карты и радио модема", "SIM cards and modem radio") }
                    TextSwitch {
                        text: app.tr("SIM-карты включены", "SIM cards on")
                        automaticCheck: false
                        checked: !app.flight
                        enabled: !app.busy
                        onClicked: app.request(app.flight ? "flight-off" : "flight-on",
                                               app.flight ? app.tr("SIM-карты включены, регистрация ~30 с", "SIM cards are on, registering (~30 s)")
                                                          : app.tr("SIM-карты и радио модема выключены", "SIM cards and the modem radio are off"))
                    }

                    Item { width: 1; height: Theme.paddingLarge }
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: Theme.paddingMedium
                        visible: app.busy
                        BusyIndicator { size: BusyIndicatorSize.Small; running: app.busy; anchors.verticalCenter: parent.verticalCenter }
                        Label { text: app.tr("Подождите…", "Please wait…"); anchors.verticalCenter: parent.verticalCenter }
                    }
                    Label {
                        x: Theme.horizontalPageMargin; width: parent.width - 2 * x; wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        visible: !app.busy && app.message !== ""
                        color: Theme.highlightColor
                        text: app.message
                    }
                    Item { width: 1; height: Theme.paddingLarge }
                    // Cancel: closes the window; the settings above apply at once
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - 4 * Theme.horizontalPageMargin
                        text: app.tr("Отмена", "Cancel")
                        onClicked: Qt.quit()
                    }
                }
            }
        }
    }
}
