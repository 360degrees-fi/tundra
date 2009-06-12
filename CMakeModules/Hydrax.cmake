# Find, include and link Hydrax
# Find is already called in the top-level CMakeLists

macro (FIND_HYDRAX)
endmacro (FIND_HYDRAX)

macro (INCLUDE_HYDRAX)
	if (MSVC)
		include_directories (${REX_DEP_PATH}/Hydrax/include)
		link_directories (${REX_DEP_PATH}/Hydrax/lib)
	endif (MSVC)
endmacro (INCLUDE_HYDRAX)

macro (LINK_HYDRAX)
	if (MSVC)
		target_link_libraries (${TARGET_NAME}
			optimized Hydrax
			debug Hydrax)
	endif (MSVC)
endmacro (LINK_HYDRAX)
