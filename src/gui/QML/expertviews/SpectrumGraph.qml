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
        title: qsTr("Spectrum")

        property real maxYAxis: 0

        Component.onCompleted: {
            var line = createSeries(ChartView.SeriesTypeLine, "line series", axisX, axisY)
            line.color = "#38ad6b"
            cppGUI.registerSpectrumSeries(chart.series(0));
        }

        Connections{
            target: cppGUI

            onSetSpectrumAxis: {
                if(axisY.max < Ymax) // Up scale y axis immediately if y should be bigger
                {
                    axisY.max = Ymax
                }
                else // Only for down scale
                {
                    yAxisMaxTimer.running = true
                    chart.maxYAxis = Ymax
                }

                axisX.min = Xmin
                axisX.max = Xmax
            }
        }

        ValueAxis {
            id: axisY
            min: 0
            titleText: qsTr("Amplitude")
        }


        ValueAxis {
            id: axisX
            titleText: qsTr("Frequency") + " [MHz]"
        }

        Timer {
            id: refreshTimer
            interval: 1 / 10 * 1000 // 10 Hz
            running: parent.visible ? true : false // Trigger new data only if spectrum is showed
            repeat: true
            onTriggered: {
               cppGUI.updateSpectrum();
            }
        }

        Timer {
            id: yAxisMaxTimer
            interval: 1 * 1000 // 1 s
            repeat: false
            onTriggered: {
               axisY.max = maxYAxis
            }
        }
    }
}
