set(kdev_colcon_SRCS
    colcon_manager.cpp
    colcon_import_json_job.cpp
    colcon_project_data.cpp
    colcon_build_job.cpp
)

ecm_qt_declare_logging_category(kdev_colcon_SRCS
    HEADER debug.h
    IDENTIFIER COLCON
    CATEGORY_NAME "kdevelop.plugins.kdev_colcon"
)

kdevplatform_add_plugin(kdev_colcon
    JSON kdev_colcon.json
    SOURCES ${kdev_colcon_SRCS}
)

target_link_libraries(kdev_colcon
    KDev::Interfaces
    KDev::Util
    KDev::Project
)
