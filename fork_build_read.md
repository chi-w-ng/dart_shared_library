# Dart Dynamic and Static Import Library

This is a fork of https://github.com/fuzzybinary/dart_shared_library, a dart dynamic library. This fork
added minor changes to the cmakelist.txt files so that both dynamic and static library are available. 

## Build

To build this library, we first need to build dart engine and runtime as a static library.
0. Following the build machine set up as described in https://github.com/dart-lang/sdk/blob/main/docs/Building.md.
1. download dart_shared_library:
2. From the project root, download dart source by following the direction in 
	https://github.com/dart-lang/sdk/blob/main/docs/Building.md .
	a. mkdir dart-sdk 
	b. cd dart-sdk
	# On Windows, this needs to be run in a shell with Administrator rights.
	c. fetch dart 
	d. cd sdk ( sdk directory will be created by the 'fetch dart' command)
3. The static build targets have changes, libdart is no longer a valid target, targets below seems to work:
	a. dart_engine_jit_static for jit
	b. dart_engine_aot_static for aot.
   see dart lang issue: https://github.com/dart-lang/sdk/issues/60799
   To build (release and debug, from the sdk directory) run:
   python tools\build.py --mode all --arch x64 dart_engine_jit_static
4. once dart engine is build, follow instruction for cmake in dart_shared_library project. 

Note: this instruction does not use build_helper in dart_shared_library; dart engine library is
built manually.
	