cmake_minimum_required(VERSION 3.1.0)

# define module name & interface version
set (module sc132gs)
string(TOUPPER ${module} SENSOR_NAME)

# we want to compile all .c files as default
file(GLOB libsources ${SENSOR_NAME}.c )

# set public headers, these get installed
#file(GLOB pub_headers include/*.h)

#include_directories()

add_library(${module} SHARED ${libsources})

add_custom_target(${module}.drv
                  ALL
                  COMMAND ${CMAKE_COMMAND} -E copy lib${module}.so ${LIB_ROOT}/rootfs/usr/share/vi/tuningtool/bin/${module}.drv
                  COMMAND ${CMAKE_COMMAND} -E copy lib${module}.so ${LIB_ROOT}/rootfs/usr/lib/lib${module}.so
                  COMMAND ${CMAKE_COMMAND} -E make_directory ${LIB_ROOT}/rootfs/usr/share/vi/isp/test/
                  COMMAND ${CMAKE_COMMAND} -E copy ${LIB_ROOT}/../drivers/${SENSOR_NAME}/*.xml ${LIB_ROOT}/rootfs/usr/share/vi/isp/test/
                  COMMAND ${CMAKE_COMMAND} -E copy ${LIB_ROOT}/../drivers/${SENSOR_NAME}/${SENSOR_NAME}*.txt ${LIB_ROOT}/rootfs/usr/share/vi/isp/test/
                  COMMAND ${CMAKE_COMMAND} -E copy ${LIB_ROOT}/../drivers/${SENSOR_NAME}/*.json ${LIB_ROOT}/rootfs/usr/share/vi/isp/test/
                  COMMAND ${CMAKE_COMMAND} -E copy ${LIB_ROOT}/../drivers/${SENSOR_NAME}/*.xml ${LIB_ROOT}/rootfs/usr/share/vi/tuningtool/bin/
                  COMMAND ${CMAKE_COMMAND} -E copy ${LIB_ROOT}/../drivers/${SENSOR_NAME}/${SENSOR_NAME}*.txt ${LIB_ROOT}/rootfs/usr/share/vi/tuningtool/bin/
                  COMMAND ${CMAKE_COMMAND} -E copy ${LIB_ROOT}/../drivers/${SENSOR_NAME}/*.json ${LIB_ROOT}/rootfs/usr/share/vi/tuningtool/bin/
                  DEPENDS ${module}
                  COMMENT "Copying ${module} driver module"
                  )
    
#install(FILES       ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/lib${module}.so.${${module}_INTERFACE_CURRENT}
#                  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
#                  RENAME      ${module}.drv
#                  )