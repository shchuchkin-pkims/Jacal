find_package(Qt6 REQUIRED COMPONENTS Core Quick Gui Network)

qt_add_executable(jackal
    main.cpp
    src/gamecontroller.cpp
    src/gamecontroller.h
    src/boardmodel.cpp
    src/boardmodel.h
    src/gameserver.cpp
    src/gameserver.h
    src/networkclient.cpp
    src/networkclient.h
    src/updatechecker.cpp
    src/updatechecker.h
    src/protocol.h
    qml.qrc
)

target_link_libraries(jackal PRIVATE
    jackal_core
    Qt6::Core
    Qt6::Quick
    Qt6::Gui
    Qt6::Network
)

target_include_directories(jackal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

if(ANDROID)
    set_target_properties(jackal PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()
