cmake_minimum_required(VERSION 3.16)


project(DroneGroundStation)


set(CMAKE_CXX_STANDARD 17)



find_package(Qt6 REQUIRED COMPONENTS
    Quick
    QuickControls2
    Network
    Sql
    Positioning
    Location
)



qt_standard_project_setup()



qt_add_executable(appDroneGroundStation


    main.cpp

    backend.cpp
    backend.h

    sensorbackend.cpp
    sensorbackend.h

    historydatabase.cpp
    historydatabase.h

)





qt_add_qml_module(appDroneGroundStation

    URI DroneGroundStation

    VERSION 1.0

    # Qt 6.11 MSVC Debug's QML AOT output crashes inside Qt6Qmld when the
    # telemetry context properties update rapidly.  Interpret the QML module
    # normally; the UI workload is small and stability is more important.
    NO_CACHEGEN


    QML_FILES

        Main.qml

        MainPage.qml

        LoginPage.qml

        Dashboard.qml

        MapView.qml

        FlightCurve.qml

        AttitudeIndicator.qml

        TelemetryPanel.qml

        Stm32Page.qml

        Stm32MapView.qml

        Stm32FlightCurve.qml

        HistoryPage.qml
        HistoryTable.qml
        HistoryFlightPath.qml
        CoordinateChart.qml
        AltitudeChart.qml
        TemperatureHumidityChart.qml
        StatusPanel.qml

)



target_link_libraries(appDroneGroundStation

    PRIVATE

    Qt6::Quick

    Qt6::QuickControls2

    Qt6::Network

    Qt6::Sql

    Qt6::Positioning

    Qt6::Location

)

qt_add_resources(appDroneGroundStation "gps_web_page"
    PREFIX "/"
    FILES
        index.html
)
