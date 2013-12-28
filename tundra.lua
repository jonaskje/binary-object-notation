Build {
	Configs = {
		Config {
			Name = "generic-gcc",
			DefaultOnHost = "linux",
			Tools = { "gcc" },
		},
		Config {
			Name = "macosx-clang",
			DefaultOnHost = "macosx",
			Tools = { "clang-osx" },
		},
		Config {
			Name = "win64-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc-vs2008"; TargetPlatform = "x64" },
		},
	},
	Units = function()
		StaticLibrary {
			Name = "Bon",
			Sources = { "src/Bon.c", "src/BonConvert.c" },
		}
		Program {
			Name = "BonTest",
			Sources = { "test/BonTest.c" },
			Includes = { "src" },
			Depends = { "Bon" },
		}
		Program {
			Name = "Json2Bon",
			Sources = { "tools/BonTools.c" },
			Includes = { "src" },
			Depends = { "Bon" },
			Defines = { "BONTOOL_JSON2BON" },
		}
		Program {
			Name = "Bon2Json",
			Sources = { "tools/BonTools.c" },
			Includes = { "src" },
			Depends = { "Bon" },
			Defines = { "BONTOOL_BON2JSON" },
		}
		Program {
			Name = "DumpBon",
			Sources = { "tools/BonTools.c" },
			Includes = { "src" },
			Depends = { "Bon" },
			Defines = { "BONTOOL_DUMPBON" },
		}

		Default "BonTest"
		Default "Json2Bon"
		Default "Bon2Json"
		Default "DumpBon"
	end,
}

