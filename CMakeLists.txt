# CMakeList.txt : CMake project for vulkan-tinker, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.14)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("vulkan-tinker")

# Add source to this project's executable.
add_executable (vulkan-tinker "src/main.cpp")

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(vulkan-tinker PRIVATE glfw)

find_package(Vulkan REQUIRED)
target_link_libraries(vulkan-tinker PRIVATE Vulkan::Vulkan)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET vulkan-tinker PROPERTY CXX_STANDARD 20)
endif()

find_program(GLSLC_EXECUTABLE glslc REQUIRED)

function(compile_shader SRC)
	get_filename_component(SHADER_NAME ${SRC} NAME)
	set(SPV "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv")
	add_custom_command(
		OUTPUT ${SPV}
		COMMAND ${GLSLC_EXECUTABLE} -o ${SPV} ${SRC}
		DEPENDS ${SRC}
		COMMENT "Compiling shader ${SRC}"
	)
	set(${ARGV1} ${SPV} PARENT_SCOPE)
endfunction()

compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/main.vert MAIN_VERT)
compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/main.frag MAIN_FRAG)
add_custom_target(shaders ALL DEPENDS ${MAIN_VERT} ${MAIN_FRAG})