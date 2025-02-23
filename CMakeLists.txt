# CMakeList.txt: memory1 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("memory1")

# 将源代码添加到此项目的可执行文件。
# 编译单元测试可执行文件
add_executable(memory1 
    test/UnitTest.cpp     # 测试入口（含main函数）
    src/memory.cpp        # 被测试的代码
    include/memory.h
)

add_executable(testCorrectness 
test/testCorrectness.cpp
    src/memory.cpp        
    include/memory.h)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET memory1 PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。
