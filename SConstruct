#!python
import os

opts = Variables([], ARGUMENTS)

opts.Add(PathVariable("redot_api_dir", "Binding ABI snapshot (26.2 is used for both Redot 26.2 and 26.3).", "redot-api/26.2", PathVariable.PathIsDir))
opts.Add(PathVariable("steam_sdk_path", "Steamworks SDK root containing public/ and redistributable_bin/.", "godotsteam/sdk", PathVariable.PathIsDir))
opts.Add(BoolVariable("include_steam_runtime", "Copy the Steam API redistributable into the build output for local testing. Public packages leave this disabled.", False))
opts.Add("linux_cross_zig", "Optional Zig executable used to cross-compile Linux binaries from Windows.", "")
opts.Add("linux_cross_glibc", "Minimum glibc version for Zig Linux cross-builds.", "2.27")
opts.Add("linux_cross_prefix", "Optional GNU compiler prefix used to cross-compile Linux binaries (for example aarch64-linux-gnu-).", "")

# Set godot-cpp defaults
requested_platform = ARGUMENTS.get("platform", "")
linux_cross_zig = ARGUMENTS.get("linux_cross_zig", "")
if os.name == "nt" and requested_platform == "linux" and linux_cross_zig:
	env = Environment(tools=["gcc", "g++", "gnulink", "ar"], PLATFORM="win32")
	for key in ("APPDATA", "COMSPEC", "LOCALAPPDATA", "SystemRoot", "USERPROFILE", "TEMP", "TMP"):
		if key in os.environ:
			env["ENV"][key] = os.environ[key]
	env["ENV"]["ZIG_GLOBAL_CACHE_DIR"] = os.path.abspath(".work/zig-cache/global")
	env["ENV"]["ZIG_LOCAL_CACHE_DIR"] = os.path.abspath(".work/zig-cache/local")
else:
	env = Environment()
env["api_version"] = "4.4"
opts.Update(env)

if requested_platform == "linux" and env["linux_cross_prefix"]:
	prefix = env["linux_cross_prefix"]
	env["CC"] = prefix + "gcc"
	env["CXX"] = prefix + "g++"
	env["AR"] = prefix + "gcc-ar"
	env["RANLIB"] = prefix + "gcc-ranlib"

if os.name == "nt" and requested_platform == "linux" and env["linux_cross_zig"]:
	zig = os.path.abspath(env["linux_cross_zig"])
	if not os.path.isfile(zig):
		print("ERROR: Zig cross-compiler not found: {}".format(zig))
		Exit(2)
	env.PrependENVPath("PATH", os.path.dirname(zig))
	env.PrependENVPath("PATH", os.path.abspath("scripts/toolchains"))
	env["ENV"]["REDOTSTEAM_ZIG"] = zig
	arch = ARGUMENTS.get("arch", "x86_64")
	zig_arch = {"x86_64": "x86_64", "arm64": "aarch64"}.get(arch)
	if zig_arch is None:
		print("ERROR: Zig cross-build supports only x86_64 and arm64, not '{}'.".format(arch))
		Exit(2)
	target = "{}-linux-gnu.{}".format(zig_arch, env["linux_cross_glibc"])
	env["CC"] = "zig.exe cc -target {}".format(target)
	env["CXX"] = "zig.exe c++ -target {}".format(target)
	env["AR"] = "redotsteam-zig-ar.cmd"
	env["RANLIB"] = "zig.exe ranlib"

redot_api_dir = env["redot_api_dir"]
redot_api_file = os.path.join(redot_api_dir, "extension_api.json")
redot_interface_file = os.path.join(redot_api_dir, "gdextension_interface.h")
if not os.path.isfile(redot_api_file) or not os.path.isfile(redot_interface_file):
	print("ERROR: Redot API inputs are missing from '{}'. Run scripts/capture_redot_api.ps1 first.".format(redot_api_dir))
	Exit(2)
env["gdextension_dir"] = redot_api_dir
Export("env")

# Import env from godot-cpp
plugin_argument_names = (
	"redot_api_dir",
	"steam_sdk_path",
	"include_steam_runtime",
	"linux_cross_zig",
	"linux_cross_glibc",
	"linux_cross_prefix",
	"target_path",
	"target_name",
)
plugin_arguments = {key: ARGUMENTS.pop(key) for key in plugin_argument_names if key in ARGUMENTS}
env = SConscript("godot-cpp/SConstruct")
ARGUMENTS.update(plugin_arguments)

# Keep source and SDK locations out of __FILE__ strings and debug information.
# The returned environment is also used by the godot-cpp library builders.
for source_root, mapped_root in ((os.path.abspath("."), "."), (os.path.abspath(env["steam_sdk_path"]), "steamworks-sdk")):
	if env.get("is_msvc", False):
		env.Append(CCFLAGS=["/d1trimfile:" + source_root + os.sep])
	else:
		env.Append(CCFLAGS=["-ffile-prefix-map=" + source_root.replace("\\", "/") + "=" + mapped_root])
if env.get("is_msvc", False):
	# The linker otherwise records the absolute PDB location in the DLL.
	env.Append(LINKFLAGS=["/PDBALTPATH:%_PDB%"])

if os.name == "nt" and requested_platform == "linux" and env["linux_cross_zig"]:
	# Zig's target triple already selects the architecture; Godot's GCC-specific
	# x86 spelling is interpreted differently by Zig's Clang driver.
	for variable in ("CCFLAGS", "LINKFLAGS"):
		env[variable] = [
			flag for flag in env[variable]
			if flag not in ("-m64", "-march=x86-64", "-march=armv8-a")
		]
	# The generated Redot binding archive has thousands of object files and
	# exceeds Windows' command-line limit. Zig ar accepts SCons response files.
	env["ARCOM"] = "${TEMPFILE('$AR $ARFLAGS $TARGET $SOURCES', '$ARCOMSTR')}"

# Define our options
opts.Add(PathVariable("target_path", "The path where the plugin is installed.", "bin/", PathVariable.PathAccept))
opts.Add(PathVariable("target_name", "The library name.", "godotsteam", PathVariable.PathAccept))
opts.Update(env)

# Local dependency paths, adapt them to your setup
steam_sdk_path = env["steam_sdk_path"]
steam_public_path = os.path.join(steam_sdk_path, "public")
steam_lib_path = os.path.join(steam_sdk_path, "redistributable_bin")

if not os.path.isfile(os.path.join(steam_public_path, "steam", "steam_api.h")):
	print("ERROR: Steamworks SDK headers are missing. Supply your own SDK with public/steam/steam_api.h under '{}' or pass steam_sdk_path=<path>.".format(steam_sdk_path))
	Exit(2)

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
env.Append(CPPPATH=[steam_public_path])
steamworks_link_name = os.path.splitext(steamworks_library)[0]
if steamworks_link_name.startswith("lib"):
	steamworks_link_name = steamworks_link_name[3:]
env.Append(LIBS=[steamworks_link_name])

# Tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["godotsteam/"])
sources = Glob("godotsteam/*.cpp")
plugin_env = env.Clone()
if plugin_env["CC"] == "cl":
	# Some public SDK mirrors normalize line endings without preserving the original
	# comment encoding. This warning is confined to comments in Valve headers.
	plugin_env.Append(CCFLAGS=["/wd4828"])

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
if os.name == "nt" and requested_platform == "linux" and env["linux_cross_zig"]:
	env["SHLIBPREFIX"] = "lib"
	env["SHLIBSUFFIX"] = ".so"
	env["SHOBJSUFFIX"] = env["suffix"] + ".o"
	plugin_env["SHLIBPREFIX"] = env["SHLIBPREFIX"]
	plugin_env["SHLIBSUFFIX"] = env["SHLIBSUFFIX"]
	plugin_env["SHOBJSUFFIX"] = env["SHOBJSUFFIX"]
else:
	env["SHOBJSUFFIX"] = env["suffix"] + env["SHOBJSUFFIX"]

libdir = {
	"android-arm64": "androidarm64",
	"linux-x86_64": "linux64",
	"linux-x86_32": "linux32",
	"linux-arm64": "linuxarm64",
	"macos-universal": "osx",
	"macos-x86_64": "osx",
	"macos-arm64": "osx",
	"windows-x86_64": "win64",
	"windows-x86_32": "win32",
}.get("{}-{}".format(env["platform"], env["arch"]), "lib")
extpath = "{}/addons/{}".format(env["target_path"], env["target_name"])

library = plugin_env.SharedLibrary(
	"{}/{}/libgodotsteam{}{}".format(extpath, libdir, env["suffix"], env["SHLIBSUFFIX"]),
	source=sources,
)

Default(library)

ext = env.InstallAs(extpath + "/godotsteam.gdextension", "godotsteam.gdextension")

Default(ext)

if env["include_steam_runtime"]:
	redist_source = os.path.join(steam_lib_path, steamworks_library)
	if not os.path.isfile(redist_source):
		print("ERROR: Steam runtime '{}' is missing. Supply the matching SDK redistributable or disable include_steam_runtime.".format(redist_source))
		Exit(2)
	redist = env.InstallAs("{}/{}/{}".format(extpath, libdir, steamworks_library), redist_source)
	Default(redist)

plugin = env.Install(env.Dir(extpath), env.Glob("plugin/*"))
plugin += env.Install(env.Dir(extpath), "plugin/editor")

Default(plugin)

notices = [
	env.InstallAs(extpath + "/LICENSE.txt", "license.md"),
	env.InstallAs(extpath + "/README.md", "REDOTSTEAM.md"),
	env.InstallAs(extpath + "/THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES.md"),
	env.InstallAs(extpath + "/tools/provision_steam_runtime.ps1", "scripts/provision_steam_runtime.ps1"),
]
Default(notices)

# Generates help for the -h scons option.
Help(opts.GenerateHelpText(env))
