
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

# Replace TMDS with 10 bit UART (same baud rate):
# add_definitions(-DDVI_SERIAL_DEBUG=1)
# add_definitions(-DRUN_FROM_CRYSTAL)

add_executable(gtia2dvi
	main.c
)

pico_generate_pio_header(gtia2dvi ${CMAKE_CURRENT_LIST_DIR}/pio/gtia_luma_ng.pio)
pico_generate_pio_header(gtia2dvi ${CMAKE_CURRENT_LIST_DIR}/pio/gtia_chroma_ng.pio)
pico_generate_pio_header(gtia2dvi ${CMAKE_CURRENT_LIST_DIR}/pio/gtia_dump.pio)

target_compile_definitions(gtia2dvi PRIVATE
	DVI_DEFAULT_SERIAL_CONFIG=${DVI_DEFAULT_SERIAL_CONFIG}
	)

target_link_libraries(gtia2dvi 
	pico_stdlib
	pico_multicore
	hardware_flash
	libdvi
)
# create map/bin/hex file etc.
pico_add_extra_outputs(gtia2dvi)

#pico_set_binary_type(gtia2dvi no_flash)


# Ścieżka do pliku nagłówkowego
set(GIT_INFO_HEADER "${CMAKE_CURRENT_LIST_DIR}/git_info.h")

# Generuj plik nagłówkowy
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/git_info.h.in
  ${GIT_INFO_HEADER}
  @ONLY
)