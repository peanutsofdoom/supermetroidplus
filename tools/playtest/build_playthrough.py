"""Build the experimental host separately, reusing unchanged GUI/core objects."""
from pathlib import Path
import subprocess,shlex,hashlib,json,argparse
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/playtest/playthrough-20260926-v1'
GUI=ROOT/'development/bsnes-hd-gui/bsnes'
CC=ROOT/'build/tools/w64devkit/bin/g++.exe'
def main():
    global OUT
    p=argparse.ArgumentParser();p.add_argument('--optimize-host',action='store_true');p.add_argument('--enemies',action='store_true');p.add_argument('--activation',action='store_true');p.add_argument('--coordinates',action='store_true');p.add_argument('--revision',type=int,default=1);p.add_argument('--ppu-object',type=Path,help='Separately qualified PPU object; keep original core objects intact');a=p.parse_args()
    name=f'smp-enemies-v{a.revision}' if a.enemies else 'smp-playthrough-v1'
    if a.enemies:OUT=ROOT/f'build/playtest/enemies-20260926-v{a.revision}'
    if (OUT/(name+'.exe')).exists():raise ValueError('Preserve previous build')
    OUT.mkdir(parents=True,exist_ok=True)
    commands=[]
    def run(args):
        args=['-O3' if a.optimize_host and x=='-O2' else x for x in args]
        cmd=[str(CC),*map(str,args)];commands.append(cmd)
        result=subprocess.run(cmd,cwd=GUI,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        with (OUT/'build.log').open('a') as f:f.write(result.stdout)
        if result.returncode:raise RuntimeError(result.stdout)
    observer=OUT/'playthrough-observer.o'
    run(['-std=c++17','-O2','-I.','-I..','-DSMP_EXPERIMENTAL_PLAYTHROUGH=1',*(['-DSMP_EXPERIMENTAL_ENEMIES=1'] if a.enemies else []),*(['-DSMP_EXPERIMENTAL_ACTIVATION=1'] if a.activation else []),f'-DSMP_COORDINATE_BUILD={a.revision}','-c',ROOT/'tools/widescreen/host/gui_observer.cpp','-o',observer])
    link=shlex.split((ROOT/'build/playthrough-make-plan.txt').read_text().splitlines()[-1])[1:]
    link[link.index('out/smp-playthrough-v1')]=str(OUT/(name+'.exe'))
    link[link.index('obj/smp-gui-observer.o')]=str(observer)
    if True:  # State restoration resets renderer history in both playable builds.
        program=OUT/'coordinate-ui.o'
        command=next(line for line in (ROOT/'build/playthrough-make-plan.txt').read_text().splitlines() if '-c target-bsnes/program/program.cpp' in line)
        args=shlex.split(command)[1:]
        for flag in ('-MMD','-MP'):args.remove(flag)
        i=args.index('-MF');del args[i:i+2]
        args[args.index('obj/ui-program.o')]=str(program)
        run([*(['-DSMP_COORDINATES=1'] if a.coordinates else []),*args]);link[link.index('obj/ui-program.o')]=str(program)
    commandLine=OUT/'command-line.o'
    command=next(line for line in (ROOT/'build/playthrough-make-plan.txt').read_text().splitlines() if '-c target-bsnes/bsnes.cpp' in line)
    args=shlex.split(command)[1:]
    for flag in ('-MMD','-MP'):args.remove(flag)
    i=args.index('-MF');del args[i:i+2]
    args[args.index('obj/ui-bsnes.o')]=str(commandLine)
    run(args);link[link.index('obj/ui-bsnes.o')]=str(commandLine)
    # Program/Presentation layout is shared by the UI translation units. Keep
    # each release self-contained; never reuse objects with an older layout.
    uiObjects=[]
    for unit in ['input','presentation','settings','tools','resource']:
        source=f'target-bsnes/{unit}/{unit}.cpp'
        command=next(line for line in (ROOT/'build/playthrough-make-plan.txt').read_text().splitlines() if f'-c {source}' in line)
        args=shlex.split(command)[1:]
        for flag in ('-MMD','-MP'):args.remove(flag)
        i=args.index('-MF');del args[i:i+2]
        obj=OUT/f'ui-{unit}.o';args[args.index(f'obj/ui-{unit}.o')]=str(obj)
        run(args);link[link.index(f'obj/ui-{unit}.o')]=str(obj);uiObjects.append(obj)
    # The compact-upload extension is private to ruby's presentation backend;
    # rebuild it separately without replacing any pinned emulator/core objects.
    ruby=OUT/'presentation-driver.o'
    command=next(line for line in (ROOT/'build/playthrough-make-plan.txt').read_text().splitlines() if '-c ../ruby/ruby.cpp' in line)
    args=shlex.split(command)[1:]
    for flag in ('-MMD','-MP'):args.remove(flag)
    i=args.index('-MF');del args[i:i+2]
    args[args.index('obj/ruby.o')]=str(ruby)
    run(args);link[link.index('obj/ruby.o')]=str(ruby)
    if a.activation:
        run(['-std=c++17','-O2','-I.','-I..','-c',ROOT/'tools/playtest/recorder.cpp','-o',OUT/'variant-recorder.o'])
        link[link.index('obj/smp-playtest-recorder.o')]=str(OUT/'variant-recorder.o')
    if a.enemies:
        run(['-std=c++20','-O2','-c',ROOT/'tools/widescreen/host/bridge.cpp','-o',OUT/'enemy-renderer.o'])
        link[link.index('obj/smp-wide-renderer.o')]=str(OUT/'enemy-renderer.o')
    if a.ppu_object:
        if not a.ppu_object.is_file():raise ValueError("Missing qualified PPU object")
        link[link.index("obj/sfc-ppu.o")]=str(a.ppu_object.resolve())
    run(link)
    run(['-std=c++17','-O2','-municode','-mwindows','-static',f'-DSMP_PREVIEW_EXE=L"{name}.exe"',ROOT/'tools/widescreen/tv_launcher.cpp','-o',OUT/'Play.exe'])
    run(['-std=c++17','-O2',ROOT/'tools/widescreen/host/playthrough_eligibility_probe.cpp','-o',OUT/'policy-test.exe'])
    subprocess.run([str(OUT/'policy-test.exe')],check=True)
    files=[program,commandLine,ruby,observer,GUI/'target-bsnes/bsnes.cpp',GUI/'target-bsnes/program/program.hpp',GUI/'target-bsnes/program/program.cpp',GUI/'target-bsnes/program/platform.cpp',GUI/'target-bsnes/program/retest-queue.cpp',ROOT/'tools/widescreen/host/retest_queue.hpp',ROOT/'tools/widescreen/host/retest_queue_overlay.hpp',OUT/(name+'.exe'),OUT/'Play.exe',ROOT/'tools/widescreen/host/gui_observer.cpp',ROOT/'tools/widescreen/host/playthrough_eligibility.hpp',*sorted((GUI/'obj').glob('*.o')),*sorted((ROOT/'tools/widescreen/native').glob('*.hpp'))]
    files.extend(sorted((ROOT/'tools/widescreen/host').glob('*.hpp')))
    files.extend(uiObjects)
    files.extend(f for f in (GUI/'target-bsnes').rglob('*') if f.suffix in ('.cpp','.hpp'))
    if a.ppu_object:
        files.append(a.ppu_object.resolve())
        if a.ppu_object.with_suffix('.json').is_file():files.append(a.ppu_object.with_suffix('.json').resolve())
    (OUT/'build.json').write_text(json.dumps({'commands':commands,'sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}},indent=2))
    print(OUT)
if __name__=='__main__':main()
