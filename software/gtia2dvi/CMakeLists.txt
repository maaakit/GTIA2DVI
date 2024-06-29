
# Pobierz skrót commita
execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Sprawdź, czy są niezacommitowane zmiany
execute_process(
  COMMAND git diff --quiet
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE GIT_DIFF_RESULT
)

# Ustal wartość zmiennej dla niezacommitowanych zmian
if(GIT_DIFF_RESULT EQUAL 0)
  set(GIT_MODIFIED "0")
else()
  set(GIT_MODIFIED "1")
endif()

# Generuj nazwę wyjściowego pliku binarnego
set(OUTPUT_NAME "gtia2dvi_${GIT_COMMIT_HASH}")
if(GIT_MODIFIED EQUAL "1")
  set(OUTPUT_NAME "${OUTPUT_NAME}_dirty")
endif()

set(BUILD_DIR "${CMAKE_BINARY_DIR}/gtia2dvi")

if(EXISTS "${BUILD_DIR}")
	# Usuń zawartość katalogu build
	file(REMOVE_RECURSE "${BUILD_DIR}")
else()
	message(WARNING "Build directory does not exist: ${BUILD_DIR}")
endif()

# Replace TMDS with 10 bit UART (same baud rate):
# add_definitions(-DDVI_SERIAL_DEBUG=1)
# add_definitions(-DRUN_FROM_CRYSTAL)

add_executable(${OUTPUT_NAME} 
	main.c
)

pico_generate_pio_header(${OUTPUT_NAME}  ${CMAKE_CURRENT_LIST_DIR}/pio/gtia_luma_ng.pio)
pico_generate_pio_header(${OUTPUT_NAME}  ${CMAKE_CURRENT_LIST_DIR}/pio/gtia_chroma_ng.pio)
pico_generate_pio_header(${OUTPUT_NAME}  ${CMAKE_CURRENT_LIST_DIR}/pio/gtia_dump.pio)

target_compile_definitions(${OUTPUT_NAME}  PRIVATE
	DVI_DEFAULT_SERIAL_CONFIG=${DVI_DEFAULT_SERIAL_CONFIG}
	)

target_link_libraries(${OUTPUT_NAME}  
	pico_stdlib
	pico_multicore
	hardware_flash
	libdvi
)
# create map/bin/hex file etc.
pico_add_extra_outputs(${OUTPUT_NAME} )

#pico_set_binary_type(${OUTPUT_NAME}  no_flash)



# Ścieżka do pliku nagłówkowego
set(GIT_INFO_HEADER "${CMAKE_CURRENT_LIST_DIR}/git_info.h")

# Generuj plik nagłówkowy
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/git_info.h.in
  ${GIT_INFO_HEADER}
  @ONLY
)