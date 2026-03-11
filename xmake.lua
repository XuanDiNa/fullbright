add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

if is_plat("windows") then
    add_requires("detours v4.0.1-xmake.1")
end

target("FullBright")
    set_kind("shared")
    set_languages("c++17")
    add_files("src/*.cpp")

    if is_plat("windows") then
        add_packages("detours")
        add_syslinks("user32")
        add_syslinks("gdi32")
    end

    after_build(function(target)
        print("FullBright built successfully")
    end)
