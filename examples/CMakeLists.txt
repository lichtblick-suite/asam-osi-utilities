# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
function(configure_example TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME} ${SOURCE_FILE})

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )

    if (LINK_WITH_SHARED_OSI)
        target_link_libraries(${TARGET_NAME} PRIVATE open_simulation_interface::open_simulation_interface)
    else ()
        target_link_libraries(${TARGET_NAME} PRIVATE open_simulation_interface::open_simulation_interface_pic)
    endif ()

    target_link_libraries(${TARGET_NAME} PRIVATE OSIUtilities)
    target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
endfunction()

# Add examples using the function above
configure_example(example_mcap_writer example_mcap_writer.cpp)
configure_example(example_mcap_reader example_mcap_reader.cpp)
configure_example(example_single_channel_binary_reader example_single_channel_binary_reader.cpp)
configure_example(example_single_channel_binary_writer example_single_channel_binary_writer.cpp)
configure_example(example_txth_reader example_txth_reader.cpp)
configure_example(example_txth_writer example_txth_writer.cpp)
configure_example(convert_osi2mcap convert_osi2mcap.cpp)
