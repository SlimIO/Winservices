{
    "targets": [
        {
            "target_name": "winservices",
            "sources": [
                "winservices.cpp"
            ],
            "libraries": [
                "Advapi32.lib"
            ],
            "include_dirs": [
                "include",
                "<!@(node -p \"require('node-addon-api').include\")"
            ],
            "dependencies": [
                "<!(node -p \"require('node-addon-api').gyp\")"
            ],
            "cflags!": [ "-fno-exceptions" ],
            "cflags_cc!": [ "-fno-exceptions" ],
            "msvs_settings": {
                "VCCLCompilerTool": {
                    "ExceptionHandling": 1
                }
            }
        }
    ]
}
