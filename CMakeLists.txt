cmake_minimum_required(VERSION 3.24)
project(DesiertoSeguro VERSION 0.3.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "DesiertoSeguro requiere Windows, Visual Studio 2022 y plataforma x64. Sigue docs/COMPILAR.md o usa GitHub Actions.")
endif()

# Las dependencias están incluidas y fijadas por commit; configurar no descarga nada.
enable_language(ASM_MASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Dependencias estáticas" FORCE)
add_subdirectory(terceros/minhook EXCLUDE_FROM_ALL)

set(DS_IMGUI "${CMAKE_CURRENT_SOURCE_DIR}/terceros/imgui")
add_library(ds_imgui STATIC
    "${DS_IMGUI}/imgui.cpp"
    "${DS_IMGUI}/imgui_draw.cpp"
    "${DS_IMGUI}/imgui_tables.cpp"
    "${DS_IMGUI}/imgui_widgets.cpp"
    "${DS_IMGUI}/misc/cpp/imgui_stdlib.cpp"
    "${DS_IMGUI}/backends/imgui_impl_win32.cpp"
    "${DS_IMGUI}/backends/imgui_impl_dx12.cpp"
)
target_include_directories(ds_imgui PUBLIC "${DS_IMGUI}" "${DS_IMGUI}/backends" "${DS_IMGUI}/misc/cpp")
target_compile_definitions(ds_imgui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(ds_imgui PRIVATE /utf-8 /EHsc)

add_library(DesiertoSeguro SHARED
    src/main.cpp
    src/render.cpp
    src/memoria.cpp
    src/configuracion.cpp
    src/funciones.cpp
    src/menu.cpp
    src/vitales.cpp
    src/mundo.cpp
    src/inventario.cpp
    src/movimiento.cpp
    src/planeo_nativo.cpp
    src/equipo_nativo.cpp
    src/monturas_nivel5.cpp
    src/monturas5_gateway.asm
    src/equipo.cpp
    src/clima.cpp
    src/delitos.cpp
    src/regiones.cpp
    src/motor.cpp
)
target_include_directories(DesiertoSeguro PRIVATE src)
target_compile_definitions(DesiertoSeguro PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(DesiertoSeguro PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/utf-8;/EHsc;/permissive-;/Zc:__cplusplus>")
target_link_libraries(DesiertoSeguro PRIVATE ds_imgui minhook d3d12 dxgi dxguid d3dcompiler xinput9_1_0 psapi bcrypt gdi32 dwmapi imm32 user32)
set_target_properties(DesiertoSeguro PROPERTIES PREFIX "" SUFFIX ".asi")

include(CTest)
if(BUILD_TESTING)
    add_executable(pruebas_patron pruebas/patron.cpp)
    target_include_directories(pruebas_patron PRIVATE src)
    target_compile_options(pruebas_patron PRIVATE /W4 /utf-8 /EHsc)
    add_test(NAME Firmas_y_limites COMMAND pruebas_patron)
    find_program(DS_POWERSHELL NAMES powershell pwsh REQUIRED)
    add_test(NAME Integridad_y_formato_ASI
        COMMAND "${DS_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verificar.ps1"
            -Raiz "${CMAKE_CURRENT_SOURCE_DIR}"
            -Archivo "$<TARGET_FILE:DesiertoSeguro>")
endif()
