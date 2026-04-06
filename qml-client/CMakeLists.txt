find_package(Qt6 REQUIRED COMPONENTS Core Quick Gui)

qt_add_executable(jackal
    main.cpp
    src/gamecontroller.cpp
    src/gamecontroller.h
    src/boardmodel.cpp
    src/boardmodel.h
    qml.qrc
)

target_link_libraries(jackal PRIVATE
    jackal_core
    Qt6::Core
    Qt6::Quick
    Qt6::Gui
)

target_include_directories(jackal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

# Android-specific settings
if(ANDROID)
    set_target_properties(jackal PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()
