if(LITO_CMAKE_DEPENDENCY_MODE STREQUAL "source")
  if(NOT QML_MATERIAL_BUILD_TYPE STREQUAL "STATIC")
    message(FATAL_ERROR "qml-material source must be built as a static QML module")
  endif()

  add_subdirectory("${LITO_CMAKE_DEPENDENCY_SOURCE_DIR}"
                   "${CMAKE_CURRENT_BINARY_DIR}/qml-material")

  get_target_property(_waywallen_qml_material_plugin_target
                      qml_material::qml_material QT_QML_MODULE_PLUGIN_TARGET)
  if(NOT TARGET "${_waywallen_qml_material_plugin_target}")
    message(FATAL_ERROR "qml-material static QML plugin target is unavailable")
  endif()
elseif(LITO_CMAKE_DEPENDENCY_MODE STREQUAL "find")
  find_package(qml_material REQUIRED)
elseif(NOT DEFINED LITO_CMAKE_DEPENDENCY_MODE)
  message(FATAL_ERROR "qml-material dependency mode is unset")
else()
  message(FATAL_ERROR
          "unsupported qml-material dependency mode '${LITO_CMAKE_DEPENDENCY_MODE}'")
endif()

if(NOT TARGET qml_material::qml_material)
  message(FATAL_ERROR "qml-material did not provide qml_material::qml_material")
endif()

add_library(waywallen-qml-material INTERFACE)
target_link_libraries(waywallen-qml-material INTERFACE qml_material::qml_material)
if(LITO_CMAKE_DEPENDENCY_MODE STREQUAL "source")
  target_link_libraries(waywallen-qml-material INTERFACE
                        "${_waywallen_qml_material_plugin_target}")
endif()
add_library(qml_material::waywallen ALIAS waywallen-qml-material)

unset(_waywallen_qml_material_plugin_target)
