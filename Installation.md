# Simple Augmented Reality Sandbox Installation

Follow these instructions to install the Augmented Reality Sandbox with
standard settings. These instructions assume that you have already
installed the [Vrui VR Toolkit](https://github.com/vrui-vr/vrui) and
the [Kinect 3D Video Package](https://github.com/vrui-vr/kinect),
following the simple installation instructions included with those
respective packages.

In the following, when asked to enter something into a terminal, each
line you are supposed to enter starts with a `$` denoting the
terminal's command prompt. Do *not* enter that `$`, but enter
everything that follows, and end each line by pressing the Enter key.

Angle brackets `<>` in commands below are placeholders, meaning that
you have to replace everything between, and including, the angle
brackets with some text that depends on your specific circumstances.
For example, if your host has eight CPUs, instead of entering
`-j<number of CPUs>` as part of some command, you would enter `-j8`.

## Step 0: Download The Augmented Reality Sandbox Repository From GitHub

If you are reading this, you probably have already done this. :) If
not, download the entire Augmented Reality Sandbox repository from
github, either by cloning the repository using `git clone`, or by
downloading and unpacking a zip file.

### Downloading And Unpacking A Zip File From GitHub

If you are unfamiliar with git and/or GitHub, you should probably go
the zip file route. On [the Augmented Reality Sandbox repository's main
page](https://github.com/vrui-vr/arsandbox), click on the green "<>
Code" button, and then click on "Download ZIP" in the menu that pops up
in response. Depending on your browser settings, you may be asked where
to store the file being downloaded, or it might be stored in a default
location such as your `Downloads` directory. Either way, take note of
what the zip file is called, and where it is stored.

Then, once the file is completely downloaded, enter the following line
into a terminal window:
```
$ cd ~/src
```
assuming that you already created the `src` directory according to
Vrui's installation instructions.

Then enter into the same terminal window:
```
$ unzip <path to downloaded zip file>
```
where you replace `<path to downloaded zip file>` with the full path to
the zip file, for example `~/Downloads/kinect-main.zip`.

Finally, check for the name of your new ARSandbox directory by entering:
```
$ ls
```
which will list all files in the `src` directory, which should include
a new directory called something like `arsandbox-main`. Take note of
this name, and then enter into that directory by entering into the
terminal window:
```
$ cd <ARSandbox directory>
```
where you replace `<ARSandbox directory>` with the name of the
directory where you cloned/unpacked the Augmented Reality Sandbox in
the previous step, as printed by `ls`.

## Step 1: Build The Augmented Reality Sandbox

To build the Augmented Reality Sandbox, enter into the same terminal
window:
```
$ make VRUI_MAKEDIR=<Vrui build system location>
```
where you replace `<Vrui build system location>` with the location of
Vrui's build system on your host, as described in Vrui's installation
instructions. For example, your command might end up being:
```
$ make VRUI_MAKEDIR=/usr/local/share/Vrui-13.1/make
```

You can speed up the build process if your host has multiple CPUs or CPU cores.
Instead of the above, enter into the same terminal:
```
$ make VRUI_MAKEDIR=<Vrui build system location> -j<number of cpus>
```
again replacing `<Vrui build system location>` with the location of
Vrui's build system on your host, and replacing `<number of cpus>` with
the number of CPUs or CPU cores on your host, say `-j8` if you have
eight cores. Note that there is no space between the `-j` and the
number of cores.

Once `make` is done, check that there were no error messages. The
quickest way to check whether the Augmented Reality Sandbox built
successfully is to enter the `make` command a second time, exactly as you
entered it the first time. The easiest way to repeat a previous command
is to press the "cursor up" key until the command appears on the
command line, and then press the Enter key. If everything went well
the first time, the second command will print:
```
make: Nothing to be done for 'all'.
```

## Step 2: Installing The Augmented Reality Sandbox

If built following these simple instructions, the Augmented Reality
Sandbox *does not need to be installed.* You can run the built
applications, `CalibrateProjector`, `SARndbox`, and `SARndboxClient`,
directly from the directory where you cloned/unpacked the sources.

For example, to run the main ARSandbox application, you would enter
the following into a terminal window:
```
$ cd <ARSandbox directory>
$ ./bin/SARndbox
```
where you replace `<ARSandbox directory>` with the full name of the
directory where you cloned/unpacked the Augmented Reality Sandbox
sources, for example:
```
$ cd ~/src/arsandbox-main
$ ./bin/SARndbox
```

## Advanced: Installing The Augmented Reality Sandbox Elsewhere

If you want to run the Augmented Reality Sandbox from multiple user
accounts, or from a dedicated user account with limited access rights,
you need to install it in a system location where it can be accessed by
those user accounts. For example, you could install it in the same
`/usr/local` directory hierarchy where you installed the Vrui VR
Toolkit following the simple instructions included with Vrui.

To do this, you need to change the build command shown in Step 1.

Start by entering the Augmented Reality Sandbox directory:
```
$ cd <ARSandbox directory>
```
replacing `<ARSandbox directory>` with the actual directory as usual.

If you already built the Augmented Reality Sandbox according to Step 1,
you first have to update its configuration. Enter into the same terminal window:
```
$ make VRUI_MAKEDIR=<Vrui build system location> INSTALLDIR=<installation location> config
```
replacing `<installation location>` with your chosen location, for
example `INSTALLDIR=/usr/local`.

Then, build the Augmented Reality Sandbox for installation in your
chosen destination by entering:
```
$ make VRUI_MAKEDIR=<Vrui build system location> INSTALLDIR=<installation location>
```
replacing `<installation location>` with the same location as in the previous command.
You can add `-j<number of cpus>` as usual, as well.

Finally, install the Augmented Reality Sandbox in your chosen location
by entering:
```
$ sudo make VRUI_MAKEDIR=<Vrui build system location> INSTALLDIR=<installation location> install
```
again using the same `<installation location>`.
