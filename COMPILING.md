# Compilation on Windows
Open SSPCommandLineC.sln using Visual Studio Express for Desktop and hit build. The resulting .exe file is your utility.

# Compilation and installation on Linux

Build the hidapi library for Linux:
- Go to the hidapi/linux folder
- Run "make -f Makefile-manual"

Install the hidapi library for system-wide availability:
- Copy the resulting library file (libhidapi-hidraw.so) to /usr/local/library
- Run "ldconfig"

Compile SSPCommandLineC:
- Go to the SSPCommandLineC folder
- Run "make"
- Copy the 99-smartstripeprobe.rules to /etc/udev/rules.d -- this makes sure that when the SmartStripeProbe is plugged in, it 
  is available for all users. If you're running on a shared system, make sure that this is what you want.

Connect your probe and run "SSPCommandLine list" to see if the probe is detected.
