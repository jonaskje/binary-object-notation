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
			Name = "win64-vs2013",
			DefaultOnHost = "windows",
			Tools = { { "msvc-vs2013";  TargetArch = "x64" } },
		},
	},
	Env = {
		CPPDEFS = { "_CRT_SECURE_NO_WARNINGS" },
		CCOPTS = {
			"/W4", "/WX",
			"/wd4100", "/wd4127", 
			{ "/O2", "/d2Zi+"; Config = "*-vs2013-release" },
		},
		GENERATE_PDB = {
			{ "1"; Config = { "*-vs2013-*" } },
		}
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
	IdeGenerationHints = {
		Msvc = {
			-- Remap config names to MSVC platform names (affects things like header scanning & debugging)
			PlatformMappings = {
				['win64-vs2013'] = 'x64',
				['win32-vs2013'] = 'Win32',
			},
			-- Remap variant names to MSVC friendly names
			VariantMappings = {
				['release']    = 'Release',
				['debug']      = 'Debug',
				['production'] = 'Production',
			},
		},
	-- Override output directory for sln/vcxproj files.
		MsvcSolutionDir = 'vs2013',
	},
}

