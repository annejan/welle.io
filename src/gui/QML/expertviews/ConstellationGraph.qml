import QtQuick 2.0
import QtCharts 2.1

// Import custom styles
import "../texts"
import "../components"

Item {
    height: Units.dp(400)
    width: Units.dp(400)

    ChartView {
        id: chart
        anchors.fill: parent

        animationOptions: ChartView.NoAnimation
        theme: ChartView.ChartThemeLight
        backgroundColor: "#00000000"
        legend.visible: false
        title: qsTr("Constellation Diagram")

        property real maxYAxis: 0

        Component.onCompleted: {
            var scatter = createSeries(ChartView.SeriesTypeScatter, "scatter series", axisX, axisY)
            scatter.markerSize = 1.0
            scatter.borderColor = "#38ad6b";
            cppGUI.registerConstellationSeries(series(0));
        }

        Connections{
            target: cppGUI

            onSetConstellationAxis: {
                axisX.min = Xmin
                axisX.max = Xmax
            }
        }

        ValueAxis {
            id: axisY
            titleText: qsTr("DQPSK Angle [Degree]")
            max: 180
            min: -180
        }

        ValueAxis {
            id: axisX
            titleText: qsTr("Subcarrier")
        }

        Timer {
            id: refreshTimer
            interval: 1 / 10 * 1000 // 10 Hz
            running: parent.visible ? true : false // Trigger new data only if spectrum is showed
            repeat: true
            onTriggered: {
               cppGUI.updateConstellation();
            }
        }

        Timer {
            id: yAxisMaxTimer
            interval: 1 * 1000 // 1 s
            repeat: false
            onTriggered: {
               axisY.max = chart.maxYAxis
            }
        }
    }
}
