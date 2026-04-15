# Software Installation

<!-- define abbreviations -->
*[ARSandbox]: Augmented Reality Sandbox

## Table of Contents

- [Software Installation](#software-installation)
    - [Table of Contents](#table-of-contents)
    - [Step 1: Install Linux](#step-1-install-linux)
    - [Step 2: Install Nvidia Driver](#step-2-install-nvidia-driver)
    - [Step 3: Install the Vrui VR Development Toolkit](#step-3-install-the-vrui-vr-development-toolkit)
        - [Step 3a: Adjusting the Screen Size](#step-3a-adjusting-the-screen-size)
    - [Step 4: Install the Kinect 3D Video Package](#step-4-install-the-kinect-3d-video-package)
    - [Step 5: Install the Augmented Reality Sandbox](#step-5-install-the-augmented-reality-sandbox)


These installation steps install Linux and the AR Sandbox software, including its underlying Vrui and Kinect packages. There is also a [full walk-through video](http://youtu.be/R0UyMeJ2pYc) of these steps, and an illustrated step-by-step guide to installing Linux Mint 19 ("Tara"). 

???+ tip
    The video is for an older version of Linux Mint as well as older versions of the Vrui, Kinect, and AR Sandbox packages; in case of any (small) discrepancies between the video and these instructions, ignore the video and follow these instructions. The illustrated guide is current. 

<!-- check links, especially for linux mint 19 install guide -->

## Step 1: Install Linux

Instructions for this step start at [0:00 in the walk-through video](https://youtu.be/R0UyMeJ2pYc?t=0m0s).

Install the 64-bit version of Linux Mint 19.3 ("Tricia") with MATE desktop environment on a blank desktop computer. This needs to be a real computer; the AR Sandbox does not work from inside a virtual machine.

<!-- red -->

If you plan to run the AR Sandbox as a **computational appliance**, i.e., a closed system with no Internet connection, keyboard, mouse, or monitor besides the projector, where the AR Sandbox application starts automatically when you power on the PC, you should prepare for this early on during installation of Linux. One of the installation steps is to create a user account on the new operating system. At that point, check the option to log into that account automatically, and do not assign a password. Then, after the installation is done, follow the optional steps in [Post-Installation Setup and Fine-Tuning](docs/installation/post_installation.md).
 <!-- steps 16, 17  -->

Here is an illustrated step-by-step guide to installing Linux Mint 19 ("Tara") on a new computer, including the extra step to enable automatic login to use the AR Sandbox as a computational appliance. There should be no differences between Linux Mint 19 and Linux Mint 19.3.


## Step 2: Install Nvidia Driver


Instructions for this step start at [13:30 in the walk-through video](https://youtu.be/R0UyMeJ2pYc?t=13m30s).

Install vendor-supplied drivers for your Nvidia graphics card. Open the Control Center, select "Driver Manager," wait for the panel to show up and the list of available drivers to be populated, and then select the recommended Nvidia binary driver and press "Apply Changes." Then wait until the change is complete (might take a while), and reboot your computer when prompted. This process is described in detail in the illustrated step-by-step guide to installing Linux Mint 19 ("Tara").

<!-- check link for linux mint 19 install guide -->

After installing the driver and rebooting the computer, check that the driver is working correctly by opening a terminal window and entering precisely the following command (if in doubt, copy&paste directly from this web page):

```sh
glxinfo | grep vendor
```

The terminal should reply with the following:

```sh
server glx vendor string: NVIDIA Corporation
client glx vendor string: NVIDIA Corporation
OpenGL vendor string: NVIDIA Corporation
```

If the reply is different, specifically, if it does not print "NVIDIA Corporation" in all three lines, something went wrong with driver installation. This issue needs to be corrected before continuing.

## Step 3: Install the Vrui VR Development Toolkit

Instructions for this step start at [20:40 in the walk-through video](https://youtu.be/R0UyMeJ2pYc?t=20m40s).

Open a terminal window and enter precisely the following commands (if in doubt, copy and paste directly from this web page, one line at a time). ~ is shorthand for the user's home directory:

<!-- update -->

```sh
cd ~
wget http://web.cs.ucdavis.edu/~okreylos/ResDev/Vrui/Build-Ubuntu.sh
bash Build-Ubuntu.sh
```

???+ tip
    The quickest way to copy and paste in Linux is to highlight the text you want to copy with the left mouse button, then move the mouse to where you want to copy the selected text, and click the middle mouse button (usually the mouse wheel).

The script run in the last command will ask you for your user's password to install prerequisite libraries, and then build the [Vrui VR Toolkit](https://vrui-vr.github.io/vrui/installation/). That will take a little while and produce lots of output, and at the end you'll be greeted by a spinning globe in a window.

<!-- check link - no blurb link ? -->

This is a good opportunity to familiarize yourself with Vrui's basic user interface. Several utilities used in subsequent installation steps, and even the AR Sandbox itself, are Vrui applications. The UI is described in some detail in [Vrui's online documentation](), specifically the "Using Vrui Applications" page, but here are the most important parts:

<!-- link to: http://web.cs.ucdavis.edu/~okreylos/ResDev/Vrui/LinkDocumentation.html -->

- **Main Menu:** Press and hold the right mouse button to pop up the main application menu. Then move the mouse to the "Rotate Earth" menu entry, and release the right mouse button. The globe should stop spinning.

- **Rotate:** While pressing and holding the left mouse button, moving the mouse will rotate the 3D view in the window around the window center.

- **Pan:** To "pan" (i.e., to move the globe left/right/up/down in the window), press and hold the "z" key on the keyboard, and move the mouse, without pressing any mouse buttons, left/right/up/down. Using keyboard keys as extra mouse buttons in this way is a common approach in Vrui applications.

- **Scale:** To "scale" (i.e., to grow or shrink the globe in the window), press and hold both the "z" key and the left mouse button, and move the mouse up/down.

- **Recenter:** Should you ever get lost (i.e., nothing shows up in the window and you don't know how to get the contents back), you can reset the view to the default by either:
    1. Selecting "Reset View" from the "View" sub-menu of the "Vrui System" sub-menu.
    2. Pressing "Win+Home," by holding the "Windows" key and then pressing the "Home" key.

- **Tool Assignment:** Now press and hold some keyboard key, let's say "1." There is no function currently assigned to that key, thus, Vrui will pop up the tool selection menu, showing many different functions that can be dynamically assigned to it. The root (top-level) tool menu contains the most important application-specific tools such as the "Extract Planes" tool used in [System Integration, Configuration, and Calibration (Step 8)](), as well as several sub-menus bundling tools with common purposes. Below is an example of assigning a tool, Curve Editor, to a key, and how to interact with the tool:
    1. Move the mouse (without pressing any other keys or buttons) to open the "Utility" sub-menu, then move to the "Curve Editor" entry, and let go of the "1" key. This will assign a Curve Editor tool to the key, and pop open the Curve Editor's control dialog. 
    2. You can move the dialog by grabbing its blue title bar with the left mouse button, and dragging it with the mouse. You can also use the left mouse button to interact with the dialog, e.g., drag the "Line Width" slider, or select different colors. 
    3. To draw curves, move the mouse back into the main window, press and hold "1," and move the mouse. Dynamically assigning functions to mouse buttons or keyboard keys is a very common operation in Vrui applications. 
    4. To delete the Curve Editor tool, move the mouse into the "tool trash can," the red rectangle in the lower-left window corner, and press and release the "1" key.

When you are done exploring, close the globe window and go back to the same terminal window as before to continue.

Once the spinning globe appeared, you can delete the Vrui installation script by running in the same terminal window:

```sh
rm ~/Build-Ubuntu.sh
```

### Step 3a: Adjusting the Screen Size

While inside the spinning globe application that was started at the end of the earlier instructions, open the application's main menu by pressing and holding the right mouse button, and check the text size of the menu items. Is the menu readable? Is the text tiny or gigantic?

A potential issue is that the Vrui toolkit tries to create a calibrated display, meaning that if an application draws a 5" line, that line should appear on the screen exactly 5" long. This feature relies on a connected display reporting its correct size to the operating system. While that works for most computer monitors, it does not work for projectors - a projector does not know its display size, as that depends on throw distance and zoom factor. As a result, projectors often report no or wildly inaccurate display sizes, and Vrui messes up by taking those at face value. In short, Vrui's automatic screen size feature might have to be disabled to work in an AR Sandbox context.

The best way to disable this feature for all Vrui applications is to edit Vrui's central configuration file, `/usr/local/etc/Vrui-8.0/Vrui.cfg`, with the `xed` text editor. In a terminal window, run:

```sh
sudo xed /usr/local/etc/Vrui-8.0/Vrui.cfg
```

Enter your password if asked to do so. Then, find the autoScreenSize true setting inside section `Window` inside section `Desktop`. In Vrui-8.0-002's configuration file, that is in line 153. 
<!-- update version -->
Then change the value of `autoScreenSize` from `true` to `false`.

With `autoScreenSize` turned off, Vrui will fall back to the screen size configured in section Screen, in lines 104-111 in the same configuration file. 
<!-- check config file lines -->
The default values in there, defining a 20" 4:3 aspect ratio monitor, should work well enough. If your projector has a different aspect ratio, say 16:9, you might want to change the size values to reflect that, otherwise the image will appear squashed. First, change the width and height settings to appropriate values, and then adjust the `origin` setting such that the center of the screen is at position `(0.0, 0.0, 0.0)`. For example, if you set `width` to `20.92` and `height` to `11.77`, for a 24" diagonal 16:9 monitor, you would set `origin` to `(-10.46, 0.0, -5.885)`. When you are done with your changes, save the file and quit the text editor.

If you have already adjusted the screen size, but still want to fine-tune the size of displayed texts, you can do so by directly changing the font size in the same configuration file. Towards the top of the file, underneath `section` `Desktop`, find the `uiFontTextHeight` settings (line 58 for Vrui-8.0-002), and change the value as desired. Text height is defined in the same unit of measurement as every other position or size in the configuration; by default, that is in inches. When done, save the file and quit the text editor.
<!-- check line -->

## Step 4: Install the Kinect 3D Video Package

Instructions for this step start at [25:48 in the walk-through video](https://youtu.be/R0UyMeJ2pYc?t=25m48s).

<!-- below needs updating -->

To install the Kinect 3D Video Package, run in the same terminal window:

``` sh
cd ~/src
wget http://web.cs.ucdavis.edu/~okreylos/ResDev/Kinect/Kinect-3.10.tar.gz
tar xfz Kinect-3.10.tar.gz
cd Kinect-3.10
make
sudo make install
sudo make installudevrules
ls /usr/local/bin
```

The sixth command might ask you to enter your password one more time; if so, enter it to continue. Check that the list of names printed by the last command contains `KinectUtil` and `RawKinectViewer`.

## Step 5:  Install the Augmented Reality Sandbox

Instructions for this step start at [27:39 in the walk-through video](https://youtu.be/R0UyMeJ2pYc?t=27m39s).


<!-- below needs updating -->

To install the Augmented Reality Sandbox software itself, run in the same terminal window:

```sh
cd ~/src
wget http://web.cs.ucdavis.edu/~okreylos/ResDev/SARndbox/SARndbox-2.8.tar.gz
tar xfz SARndbox-2.8.tar.gz
cd SARndbox-2.8
make
ls ./bin
```

Check that the list of names printed by the last command contains `CalibrateProjector`, `SARndbox`, and `SARndboxClient`. The ARSandbox calibration utility and main application are now in `~/src/SARndbox-2.8/bin`.
