message(STATUS "AddressSanitizer enabled")
target_compile_options(ctrace PRIVATE -fsanitize=address -g -g3 -O1)
target_link_options(ctrace PRIVATE -fsanitize=address)
