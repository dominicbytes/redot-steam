#!python
import os

opts = Variables([], ARGUMENTS)

# Set godot-cpp defaults
env = Environment()
env["api_version"] = "4.4"
Export("env")

# Import env from godot-cpp
env = SConscript("godot-cpp/SConstruct")

# Define our options
opts.Add(PathVariable("target_path", "The path where the plugin is installed.", "bin/", PathVariable.PathAccept))
opts.Add(PathVariable("target_name", "The library name.", "godotsteam", PathVariable.PathAccept))
opts.Update(env)

# Local dependency paths, adapt them to your setup
steam_lib_path = "godotsteam/sdk/redistributable_bin"

# Check our platform specifics
if env['platform'] in ("macos", "osx"):
	# Set the correct Steam library
	steam_lib_path += "/osx"
	steamworks_library = "libsteam_api.dylib"

elif env['platform'] in ("linuxbsd", "linux"):
	env.Append(RPATH=env.Literal("\\$$ORIGIN"))
	# Set correct Steam library
	if env['arch'] == "x86_32":
		steam_lib_path += "/linux32"
	# Used for Steam Frame / ARM64
	elif env['arch'] == "arm64":
		steam_lib_path += "/linuxarm64"
	else:
		steam_lib_path += "/linux64"
	steamworks_library = "libsteam_api.so"

# Very specifically Steam Frame / Android ARM64
elif env['platform'] == "android":
	steam_lib_path += "/androidarm64"
	steamworks_library = "libsteam_api.so"

elif env['platform'] == "windows":
	# This makes sure to keep the session environment variables on windows,
	# that way you can run scons in a vs 2017 prompt and it will find all the required tools
	# env.Append(ENV=os.environ)

	# Set correct Steam library
	steam_lib_path += "/win64" if env['arch'] == "x86_64" else ""
	steamworks_library = "steam_api64.dll" if env['arch'] == "x86_64" else "steam_api.dll"

	# If compiling with MSCV
	if env["CC"] == "cl":
		env.Append( CCFLAGS=["/bigobj"] )
	else:
		env.Append( CCFLAGS=["-Wa,-mbig-obj"] )

# Make sure our binding library is properly includes
env.Append(LIBPATH=[steam_lib_path])
env.Append(CPPPATH=["godotsteam/sdk/public"])
env.Append(LIBS=[
	steamworks_library.replace(".dll", "")
])

# Tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["godotsteam/"])
sources = Glob("godotsteam/*.cpp")

# To get the in-editor docs functioning properly
if env["target"] in ["editor", "template_debug"]:
	try:
		doc_data = env.GodotCPPDocData("godotsteam/gen/doc_data.gen.cpp", source=Glob("godotsteam/doc_classes/*.xml"))
		sources.append(doc_data)
	except AttributeError:
		print("Not including class reference as we're targeting a pre-4.3 baseline.")

# These should probably go in godot-cpp upstream
if env["platform"] == "macos":
	env["SHLIBSUFFIX"] = ".dylib"
env["SHOBJSUFFIX"] = env["suffix"] + env["SHOBJSUFFIX"]

libdir = {
	"android-arm64": "androidarm64",
	"linux-x86_64": "linux64",
	"linux-x86_32": "linux32",
	"macos-universal": "osx",
	"macos-x86_64": "osx",
	"macos-arm64": "osx",
	"windows-x86_64": "win64",
	"windows-x86_32": "win32",
}.get("{}-{}".format(env["platform"], env["arch"]), "lib")
extpath = "{}/addons/{}".format(env["target_path"], env["target_name"])

library = env.SharedLibrary(
	"{}/{}/libgodotsteam{}{}".format(extpath, libdir, env["suffix"], env["SHLIBSUFFIX"]),
	source=sources,
)

Default(library)

ext = env.InstallAs(extpath + "/godotsteam.gdextension", "godotsteam.gdextension")

Default(ext)

redist = env.InstallAs("{}/{}/{}".format(extpath, libdir, steamworks_library), steam_lib_path + "/" + steamworks_library)

Default(redist)

plugin = env.Install(env.Dir(extpath), env.Glob("plugin/*"))
plugin += env.Install(env.Dir(extpath), "plugin/editor")

Default(plugin)

# Generates help for the -h scons option.
Help(opts.GenerateHelpText(env))
