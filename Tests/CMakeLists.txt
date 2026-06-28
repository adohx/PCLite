enable_testing()

# Each module's tests live in their own subdirectory here (mirroring src/),
# rather than nested inside src/<module>/tests -- added once the module
# library targets they link against (pclite_project/converter/viewer) are
# already defined, see the root CMakeLists.txt.
add_subdirectory(project)
add_subdirectory(converter)
add_subdirectory(viewer)
