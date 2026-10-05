import QtQuick
import AuroraNcs

// One orb. Everything it shows comes from the shared `driver` (see driver.h).
Item {
    id: root

    opacity: driver.hidden ? 0 : driver.opacity
    Behavior on opacity {
        NumberAnimation { duration: 600; easing.type: Easing.InOutQuad }
    }

    NcsVisualizer {
        id: orb
        anchors.centerIn: parent
        width: Math.max(8, Math.floor(Math.min(root.width, root.height) * driver.orbScale))
        height: width
        visible: root.opacity > 0

        amplitude: driver.amplitude
        noiseOffset: driver.noiseOffset
        color: driver.color
        glowColor: driver.glowColor
        dotCount: driver.dotCount
        dotScale: driver.dotScale
        glowScale: driver.glowScale
        seed: 1234
    }

    // The renderer reports GL problems (no OpenGL 3.3, shader errors) here.
    Text {
        anchors.centerIn: parent
        width: parent.width * 0.8
        visible: orb.error !== ""
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: "#ff6b6b"
        text: "aurora-ncs: " + orb.error
    }
}
