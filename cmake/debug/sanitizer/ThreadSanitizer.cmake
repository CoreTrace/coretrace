message(STATUS "ThreadSanitizer enabled")
target_compile_options(ctrace PRIVATE -fsanitize=thread -g -g3 -O1)
target_link_options(ctrace PRIVATE -fsanitize=thread)
