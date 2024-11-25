include(GNUInstallDirs)
install( 
  TARGETS ubiquity_client_static ubiquity_client
  EXPORT UbiquityTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
  EXPORT UbiquityTargets
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ubiquity
  NAMESPACE Ubiquity::
  FILE UbiquityTargets.cmake # Not sure if this is still needed
  )

include(CMakePackageConfigHelpers)
configure_package_config_file( 
  "UbiquityConfig.cmake.in" 
  "UbiquityConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ubiquity
  PATH_VARS
    CMAKE_INSTALL_LIBDIR
  )

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/UbiquityConfigVersion.cmake
  VERSION 0.1
  COMPATIBILITY SameMajorVersion
  )

### Install Config and ConfigVersion files
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/UbiquityConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/UbiquityConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Ubiquity"
  )


