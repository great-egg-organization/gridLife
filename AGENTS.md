# Project Structure
Global configuration can be found in the root folder while the gridLife subdirectory houses most of the code.

# BUILD
 - the premade BUILDCOMMAND builds an app in Release mode with a GUI (requires SFML)
 - the premade BUILDPERFTEST builds an app in RelWithDebInfo mode without a gui (does NOT require SFML)
 - output lands in out/[OS]/[BUILD_TYPE]/gridLife
 - if needed, cmake can be invoced manually with -DCMAKE_BUILD_TYPE:STRING=... to set build type and or -Dinteractive_sim:BOOL=true to enable the GUI
 
# Tests
There shall never be tests in this repository. Checks of program validity shall be added through ADD_CHECK(lambda_that_turns_void_into_bool) or ADD_CHECK(bool).
When running in Debug mode, a breakpoint can then be set on dummyFunction(), which will attempt to halt on the ADD_CHECK run before the first one that has a lambda evaluating to false.
ADD_CHECKs with bools will cause the previous ADD_CHECK to fail if supplied with false.
 