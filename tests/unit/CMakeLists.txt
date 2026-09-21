# Unit tests for appinstalld2. Built only with -DBUILD_TESTS=ON; the binary is
# self-contained (tests/unit/testfw.h) so it cross-compiles for every target
# and can run on the device.

add_executable(appinstalld-tests
    main.cpp
    test_utils.cpp
    test_apppackage.cpp
    test_installhistory.cpp
    test_system.cpp
    ${CMAKE_SOURCE_DIR}/src/base/Logging.cpp
    ${CMAKE_SOURCE_DIR}/src/base/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/base/System.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/AppPackage.cpp
)

target_include_directories(appinstalld-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(appinstalld-tests ${EXT_LIBS})

add_test(NAME appinstalld-unit-tests COMMAND appinstalld-tests)
