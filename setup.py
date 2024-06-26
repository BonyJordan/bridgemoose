import setuptools
import os

with open("README.md", "r") as fh:
    long_description = fh.read()

module_jade = setuptools.Extension('bridgemoose.jade',
    extra_compile_args = ['-std=c++11'],
    sources=[os.path.join('jade', x) for x in [
        'Python.cpp',
        'ansolver.cpp',
        'bdt.cpp',
        'cards.cpp',
        'ddscache.cpp',
        'intset.cpp',
        'problem.cpp',
        'solutil.cpp',
        'solver.cpp',
        'state.cpp',
        'sthash.cpp',
        'xxhash.cpp',
    ]])

module_jbdd = setuptools.Extension('bridgemoose.jbdd',
    sources=["jbdd/jbdd.cpp", "jbdd/j128.cpp"])

module_dds = setuptools.Extension('bridgemoose.dds',
    # define_macros = [('DDS_THREADS_GCD', None), ('DDS_THREADS_STL', None)],
    define_macros = [('DDS_THREADS_STL', None)],
    extra_compile_args = ['-std=c++11'],
    sources=[os.path.join('dds', x) for x in [
        "ABsearch.cpp",
        "ABstats.cpp",
        "CalcTables.cpp",
        "DealerPar.cpp",
        "File.cpp",
        "Init.cpp",
        "LaterTricks.cpp",
        "Memory.cpp",
        "Moves.cpp",
        "PBN.cpp",
        "Par.cpp",
        "PlayAnalyser.cpp",
        "QuickTricks.cpp",
        "Scheduler.cpp",
        "SolveBoard.cpp",
        "SolverIF.cpp",
        "System.cpp",
        "ThreadMgr.cpp",
        "TimeStat.cpp",
        "TimeStatList.cpp",
        "Timer.cpp",
        "TimerGroup.cpp",
        "TimerList.cpp",
        "TransTableL.cpp",
        "TransTableS.cpp",
        "dds.cpp",
        "dump.cpp",
        "Python.cpp"
    ]])



setuptools.setup(
    packages=setuptools.find_packages('src'),
    package_dir={'':'src'},
    ext_modules=[module_jade, module_dds, module_jbdd],
    name="BridgeMoose",
    version="0.2",
    author="Jordan",
    author_email="jordan@jyjy.org",
    description="Analytic Tools for the card game, Bridge",
    long_description=long_description,
    classifiers=[
        "License :: OSI Approved :: MIT License"
    ],
    python_requires=">=3.5",
)

