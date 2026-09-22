import os

SetOption('num_jobs', os.cpu_count() or 4)

# Override with scons cxx=clang-cl, or hell, god forbid, scons cxx=g++
IS_NT = os.name == 'nt'
DEFAULT_CXX = 'cl' if IS_NT else 'clang++'
CXX = ARGUMENTS.get('cxx', DEFAULT_CXX)

# A person who thinks all the time, has nothing to think about except thought

# I ***REALLY*** WANTED TO USE `/Compiler:IsThisConsideredAFlagOrAFileNameToYou`
# Instead of O2 flag, but it was inconsistent soooo im using /O2 as equivalent
import subprocess

def _probe_compiler_flavor(target, source, env):
    # Genius, right? GCC will think its path, MSC will think its arg
    result = subprocess.run(
        [env['CXX'], '/O2'], capture_output=True, text=True, env=env['ENV']
    )
    with open(str(target[0]), 'w') as f:
        f.write(result.stdout + result.stderr)
    return 0


# conf_dir/log_file keyed by CXX
_safe_cxx_name = ''.join(c if c.isalnum() else '_' for c in CXX)
_probe_env = Environment(CXX=CXX, ENV=os.environ)
_conf = _probe_env.Configure(
    conf_dir=f'.sconf_temp_{_safe_cxx_name}', log_file=f'config_{_safe_cxx_name}.log'
)
_ok, _probe_output = _conf.TryAction(
    Action(_probe_compiler_flavor), text='int main(){}', extension='.cpp'
)
_probe_env = _conf.Finish()
MSVC_STYLE = _ok and 'no such file or directory' not in _probe_output.lower()

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
env.Alias('compiledb', [cdb])

if not IS_NT:
    env.Append(LIBS=['pthread'])

build_dir = 'build'
objs_dir = os.path.join(build_dir, 'objs')
models_dir = os.path.join(build_dir, 'models')

models = env.Glob('assets/models/*.obj')
copied_models = [
    env.Command(
        os.path.join(models_dir, os.path.basename(str(m))), m,
        Copy('$TARGET', '$SOURCE')
    ) for m in models
]
env.Alias('models', copied_models)

obj_format = env.Object(os.path.join(objs_dir, 'obj'), 'src/obj.cpp')

raytracer = env.Program(
    target=os.path.join(build_dir, 'raytracer'),
    source=[env.Object(os.path.join(objs_dir, 'main'), 'src/main.cpp'), obj_format],
)

bake = env.Program(
    target=os.path.join(build_dir, 'bake'),
    source=[env.Object(os.path.join(objs_dir, 'bake'), 'src/bake.cpp'), obj_format],
)

Default([raytracer, bake, cdb, copied_models])
