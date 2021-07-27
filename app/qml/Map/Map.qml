import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtWebEngine 1.3
import QtWebChannel 1.0
import Industrial.Controls 1.0 as Controls

Item {
    id: root

    WebEngineView {
        anchors.fill: parent
        url: "../../web/index.html"
        webChannel: WebChannel { id: webChannel }
        onJavaScriptConsoleMessage: console.log(message)
    }

    MapControlView {
        id: mapControl
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Controls.Theme.margins
    }
}
