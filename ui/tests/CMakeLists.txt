# Discover page-window unit tests (Qt Test).
#
# Manual QML checklist (not automated here):
# - Scroll far down then back: no contentY jump, no duplicate cards (DiscoverPage.qml)
# - Leave Discover mid-fetch and return: session cleared, reload runs
# - Large vs default window: page-2 prefetch only above 1.5x default size
# - Discover is not StackView-cached (Window.qml pageCacheable)
# - Continuous scroll: RemoteCard previews with cache:false

find_package(Qt6 REQUIRED COMPONENTS Test Core)

add_executable(waywallen-ui-discover-tests
  discover_tests.cpp
)

set_target_properties(waywallen-ui-discover-tests PROPERTIES
  CXX_SCAN_FOR_MODULES ON
  AUTOMOC ON
)

target_compile_features(waywallen-ui-discover-tests PRIVATE cxx_std_23)
target_link_libraries(waywallen-ui-discover-tests PRIVATE
  waywallen-qml
  Qt6::Test
  Qt6::Core
)

add_test(NAME waywallen-ui-discover-tests COMMAND waywallen-ui-discover-tests)
