-- include subprojects
set_config("commonlib_xbyak", true)
includes("commonlibf4")
includes("common")
add_requires("jsoncpp")
add_requires("xbyak")

-- set project constants
set_project("F4EE")
set_version("0.0.0")
set_languages("c++23")
set_warnings("all")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- define targets
target("f4ee")
    set_values("xse.plugin.name", "Fallout 4 Engine Extender")
    set_values("xse.plugin.author", "Expired6978")
    set_values("xse.plugin.description", "Fallout 4 Engine Extender")

    add_rules("commonlibf4.plugin", {
        name = "Fallout 4 Engine Extender",
        author = "Expired6978",
        description = "Fallout 4 Engine Extender"
    })

    -- add src files
    add_files("f4ee/**.cpp")
    add_headerfiles("f4ee/**.h")
    add_includedirs("f4ee")
	set_pcxxheader("f4ee/pch.h")
    add_packages("jsoncpp")
    add_packages("xbyak")
	add_deps("common")
