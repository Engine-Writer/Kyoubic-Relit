import os
import subprocess

SetOption('num_jobs', os.cpu_count() or 4)

# Override with scons cxx=clang-cl, or hell, god forbid, scons cxx=g++
DEFAULT_CXX = 'cl' if os.name == 'nt' else 'clang-cl'
CXX = ARGUMENTS.get('cxx', DEFAULT_CXX)


def detect_msvc_style(cxx):
    try:
        # GCC type compilers think /clang:-v is a path (XD)
        # So I kinda just went with that to figure out if something
        # is GCC-style or MSVC-style
        result = subprocess.run(
            [cxx, '/clang:-v'], capture_output=True, text=True, timeout=5
        )
        banner = (result.stdout + result.stderr).lower()
        if 'no such file or directory' in banner or 'no input files' in banner:
            return False
        
        if 'clang version' in banner or 'installeddir' in banner:
            return True
    except (OSError, subprocess.SubprocessError):
        pass

    # cl.exe is somehow dumber and just emits MS banner without understaning
    # neither /clang:-v nor --version flags (... bruh)
    try:
        result = subprocess.run(
            [cxx, '--version'], capture_output=True, text=True, timeout=5
        )
        banner = (result.stdout + result.stderr).lower()
        if 'microsoft' in banner:
            return True
        if 'clang version' in banner or 'gcc' in banner or 'g++' in banner:
            return False
    except (OSError, subprocess.SubprocessError):
        pass

    raise RuntimeError(
        f"detect_msvc_style: could not determine compiler flavor for '{cxx}' "
        "from either /clang:-v or --version output"
    )
    # Now you might be wondering why we are using /clang:-v 
    # rather than an MSVC-supported flag. Thats a great question.
    # Its cuz I couldnt actually find a version flag for MSVC 
    # so I will assume they dont have one for some reason (somehow)


MSVC_STYLE = detect_msvc_style(CXX)

if MSVC_STYLE:
    CCFLAGS = ['/std:c++17', '/EHsc', '/O2', '/nologo']
else:
    CCFLAGS = ['-std=c++17', '-O2']

env = Environment(
    CXX=CXX,
    CCFLAGS=CCFLAGS,
    CPPPATH=['src', 'src/include'],
    ENV=os.environ,
)

env.Tool('compilation_db')
cdb = env.CompilationDatabase('compile_commands.json')
# Unused but to match Kyoubic Engine's build system configs
cdb_c = env.CompilationDatabase('compile_commands_c.json')
env.Alias('compiledb', [cdb, cdb_c])

if not MSVC_STYLE:
    env.Append(LIBS=['pthread'])

build_dir = 'build'
objs_dir = os.path.join(build_dir, 'objs')

obj_format = env.Object(os.path.join(objs_dir, 'obj'), 'src/obj.cpp')

raytracer = env.Program(
    target=os.path.join(build_dir, 'raytracer'),
    source=[env.Object(os.path.join(objs_dir, 'main'), 'src/main.cpp'), obj_format],
)

bake = env.Program(
    target=os.path.join(build_dir, 'bake'),
    source=[env.Object(os.path.join(objs_dir, 'bake'), 'src/bake.cpp'), obj_format],
)

Default([raytracer, bake, cdb, cdb_c])
