cmake_minimum_required(VERSION 2.4)
project(vimwiki_agenda)

add_compile_options(-Wall -Wextra)

add_executable(vimwiki_agenda
	src/main.c src/vimwiki.c src/output.c
)
