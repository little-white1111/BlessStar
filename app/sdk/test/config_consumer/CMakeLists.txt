add_executable(shm_manager_test shm_manager_test.cpp)
add_executable(double_buffer_test double_buffer_test.cpp)
add_executable(eventfd_notifier_test eventfd_notifier_test.cpp)
add_executable(consumer_abi_test consumer_abi_test.cpp)
add_executable(consumer_cpp_test consumer_cpp_test.cpp)
add_executable(consumer_e2e_test consumer_e2e_test.cpp)

target_include_directories(shm_manager_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_include_directories(double_buffer_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_include_directories(eventfd_notifier_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_include_directories(consumer_abi_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_include_directories(consumer_cpp_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_include_directories(consumer_e2e_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

# Link to shm sources (they are compiled into bs_app_sdk now)
target_link_libraries(shm_manager_test PRIVATE bs_app_sdk)
target_link_libraries(double_buffer_test PRIVATE bs_app_sdk)
target_link_libraries(eventfd_notifier_test PRIVATE bs_app_sdk)
target_link_libraries(consumer_abi_test PRIVATE bs_app_sdk)
target_link_libraries(consumer_cpp_test PRIVATE bs_app_sdk)
target_link_libraries(consumer_e2e_test PRIVATE bs_app_sdk)

# Register as CTest tests
add_test(NAME shm_manager_test COMMAND shm_manager_test)
add_test(NAME double_buffer_test COMMAND double_buffer_test)
add_test(NAME eventfd_notifier_test COMMAND eventfd_notifier_test)
add_test(NAME consumer_abi_test COMMAND consumer_abi_test)
add_test(NAME consumer_cpp_test COMMAND consumer_cpp_test)
add_test(NAME consumer_e2e_test COMMAND consumer_e2e_test)
