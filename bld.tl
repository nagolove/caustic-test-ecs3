-- vim: fdm=marker
return {
    {
        --[[
        dependencies = {
            {
                disabled = false,
                build = "@build_with_cmake",
                description = "svg parsing library",
                dir = "nanosvg",
                includes = { 
                    "nanosvg/src",
                },
                libdirs = { "nanosvg" },
                links = { "nanosvg", },
                links_internal = {},
                name = "nanosvg",
                url_action = "git",
                url = "https://github.com/memononen/nanosvg.git",
            },
        },
        --]]

        artifact = "ecs-test",
        main = "main.c",
        src = "src",
        not_dependencies = {
            "lfs",
            "resvg",
            "rlwr",
        },
    },
}
