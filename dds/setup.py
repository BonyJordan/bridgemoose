import setuptools

module_dds = setuptools.Extension('bridgemoose.dds',
    define_macros = [('DDS_THREADS_GCD', None), ('DDS_THREADS_STL', None)],
    extra_compile_args = ['-std=c++11'],
    sources=[
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
    ])


setuptools.setup(name='bridgemoose.dds',
    version='1.1',
    ext_modules=[module_dds])
