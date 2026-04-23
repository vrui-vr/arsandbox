# Post-Installation Setup and Fine-Tuning

The earlier instructions get the ARSandbox software installed and running, but there are still a lot of (optional) improvements to be made. The following steps automate full-screen display and tool bindings, configure multiple displays, etc.

## Step 1: Create Per-application Configuration File Directory

The Vrui VR toolkit supports per-application configuration files, to set display or interaction options, or pre-bind regularly-used tools. These configuration files are stored in a directory `~/.config/Vrui-<version>/Applications`, and are given the same name as the application to which they belong, plus a .cfg extension. Create the configuration file directory by running in a terminal window:

```sh
mkdir -p ~/.config/Vrui-8.0/Applications
```

## Step 2: Create Configuration File for `CalibrateProjector`

Create a configuration file for the `CalibrateProjector` application using the `xed` text editor:

```sh
xed ~/.config/Vrui-8.0/Applications/CalibrateProjector.cfg
```

Into that new file, paste exactly the following text:

```sh
section Vrui
    section Desktop
        section Window
            # Force the application's window to full-screen mode:
            windowFullscreen true
        endsection

        section Tools
            section DefaultTools
                # Bind a tie point capture tool to the "1" and "2" keys:
                section CalibrationTool
                    toolClass CaptureTool
                    bindings ((Mouse, 1, 2))
                endsection
            endsection
        endsection
    endsection
endsection
```

then save the new file and exit from the text editor.

???+ tip
    You can copy & paste the above text by highlighting the entire contents of the text box with the left mouse button, then moving the mouse over into the empty text editor window, and pressing the middle mouse button.

If you now start `CalibrateProjector`, its window will cover the entire screen, with no title bars or panels remaining. If you press the ++1++ key, the program will capture a calibration tie point, and if you press the ++2++ key, it will re-capture the background, indicated by the screen turning red for two seconds.

You can replace the ++1++ and ++2++ key names in the "bindings" tag with any other keys you like.

## Step 3: Create Configuration File for `SARndbox`

Create a configuration file for the `SARndbox` application using the `xed` text editor:

```sh
xed ~/.config/Vrui-8.0/Applications/SARndbox.cfg
```

Into that new file, paste exactly the following text:

```sh
section Vrui
    section Desktop
        # Disable the screen saver:
        inhibitScreenSaver true

        section MouseAdapter
            # Hide the mouse cursor after 5 seconds of inactivity:
            mouseIdleTimeout 5.0
        endsection

        section Window
            # Force the application's window to full-screen mode:
            windowFullscreen true
        endsection

        section Tools
            section DefaultTools
                # Bind a global rain/dry tool to the "1" and "2" keys:
                section WaterTool
                    toolClass GlobalWaterTool
                    bindings ((Mouse, 1, 2))
                endsection
            endsection
        endsection
    endsection
endsection
```

Then save the new file and exit from the text editor.

As in [Step 2](#step-2-create-configuration-file-for-calibrateprojector), this will force `SARndbox` to start in full-screen mode, ensuring that the calibration created using `CalibrateProjector` exactly matches the one used in the actual ARSandbox. In addition, the "inhibitScreenSaver" setting will prevent the screen from blanking if no keys are pressed, and the "mouseIdleTimeout 5.0" setting will hide the mouse cursor after five seconds of inactivity. To get the cursor back (for menu interactions etc.), simply move the mouse. Finally, the "WaterTool" section binds a tool to add or remove water to/from the entire ARSandbox, by pressing "1" to rain, and "2" to drain. As previously, set the binding to whatever keys you prefer.

## Step 4: Create a Desktop Icon to Launch the ARSandbox

Starting the ARSandbox may require typing a lengthy command line into a terminal window, which might get tedious. To simplify things, you can create a desktop icon to launch the ARSandbox by simply double-clicking. This is best done in two steps: first, create a shell script to launch the `SARndbox` application with all command line arguments; second, link that shell script to a desktop icon.

To create a shell script, run in a terminal window:

```sh
xed ~/src/SARndbox-2.8/RunSARndbox.sh
```

This will open an editor window with an empty file. Paste exactly the following contents into that file:

```sh
#!/bin/bash

# Enter SARndbox directory:
cd ~/src/SARndbox-2.8

# Run SARndbox with proper command line arguments:
./bin/SARndbox -uhm -fpv
```

Add any command line arguments you normally use to the last line. Then save the file, exit the text editor, and run in the same terminal:

```sh
chmod a+x ~/src/SARndbox-2.8/RunSARndbox.sh
```

This will tell the operating system that the shell script can be executed.

To create a desktop icon, run in a terminal window:

```sh
xed ~/Desktop/RunSARndbox.desktop
```

Into the empty text file, paste the following contents:

```sh
#!/usr/bin/env xdg-open
[Desktop Entry]
Version=1.0
Type=Application
Terminal=false
Icon=mate-panel-launcher
Icon[en_US]=
Name[en_US]=
Exec=/home/<username>/src/SARndbox-2.8/RunSARndbox.sh
Comment[en_US]=
Name=Start the ARSandbox
Comment=
```

Replace `<username>` with your actual user name, to locate the shell script created in the previous step. Then save the file, exit the text editor, and make the file executable:

```sh
chmod a+x ~/Desktop/RunSARndbox.desktop
```

Now, double-clicking the icon will start the ARSandbox with all command line arguments.

## Step 5: Launch the ARSandbox on Login or Boot

To run the ARSandbox as a computational appliance that starts automatically when the PC is powered on, you need to create an auto-login account during Linux installation in [Step 1 in "Software Installation"](software_installation.md#step-1-install-linux), and create a shell script to run the `SARndbox` application as described in [Step 4](#step-4-create-a-desktop-icon-to-launch-the-arsandbox). Then you add that shell script to your user account's start-up list. Select the "Startup Applications" applet in the MATE Control Center, and click the "Add" button next to the list of "Additional startup programs." Into the dialog box that opens, enter a name (such as "Start ARSandbox"), and into the Command field enter the full name of the shell script, i.e., `/home/<username>/src/SARndbox-2.8/RunSARndbox.sh`, where you replace `<username>` with your actual user name.

The next time you log into your account, or the next time the PC powers up (if auto-login is enabled), the ARSandbox will start automatically. To exit from the application, for example to run a new projector calibration or do other maintenance tasks, simply press the ++esc++ key.

## Step 6: Use Multiple Screens

If you are running an ARSandbox from a laptop, or if your desktop computer has a main monitor in addition to the sandbox projector, you can tell `CalibrateProjector` and `SARndbox` to use the projector, and leave the main monitor for other purposes, such as starting scripts or displaying a secondary view of the virtual topographic map from arbitrary points of view. First, determine to which video output port the sandbox projector is connected. In a terminal window, run:

```sh
xrandr | grep connected
```

The `xrandr` utility will print a list of all video output ports that exist on the computer, and information about the monitors/projectors connected to those ports. An `xrandr` report might look like this:

```sh
DVI-I-0 disconnected primary (normal left inverted right x axis y axis)
DVI-I-1 connected 2560x1600+0+0 (normal left inverted right x axis y axis) 641mm x 401mm
HDMI-0 disconnected (normal left inverted right x axis y axis)
DP-0 disconnected (normal left inverted right x axis y axis)
DVI-D-0 connected 1600x1200+2560+0 (normal left inverted right x axis y axis) 367mm x 275mm
DP-1 disconnected (normal left inverted right x axis y axis)
```

This report shows two connected monitors: One with 2560x1600 pixels connected to port DVI-I-1, and a secondary with 1600x1200 pixels and origin +2560+0, i.e., positioned to the right of the main monitor, connected to port DVI-D-0.

From your `xrandr` report, determine the port name connected to your sandbox projector, for example by looking for the projector's pixel size, e.g., 1024x768. If your projector is connected via an HDMI cable, the port name will usually be something like HDMI-0. Then direct `CalibrateProjector` and `SARndbox` to open their display windows on that video output port by editing their respective configuration files:

```sh
xed ~/.config/Vrui-8.0/Applications/CalibrateProjector.cfg
and afterwards
xed ~/.config/Vrui-8.0/Applications/SARndbox.cfg
In both files, insert the following "outputName" setting into the existing "Window" section:
        section Window
            ...
            # Open the window on a specific video output port:
            outputName <port name>
            ...
        endsection
```

where `<port name>` is replaced with the name of your projector's actual video output port from your `xrandr` report, e.g., HDMI-0.

Afterwards, starting `CalibrateProjector` or `SARndbox` will send their display windows to the sandbox projector and full-screen them there. Check that there are no remaining window frames or panels etc.

The `xrandr` utility can also be used to turn connected monitors/projectors on or off. For example, let's say that your main monitor is connected to port DVI-I-1, and your projector to port HDMI-0. Then you can turn on the projector, and place it to the right of your main monitor, by running:

```sh
xrandr --output DVI-I-1 --auto --primary --output HDMI-0 --auto --right-of DVI-I-1
To turn the projector off again, run:
xrandr --output DVI-I-1 --auto --primary --output HDMI-0 --off
```

## Step 7: Show a Secondary View of the ARSandbox

If you have multiple displays connected to the PC running your ARSandbox, and have done the multi-screen setup in [Step 6](#step-6-use-multiple-screens), then you can show a second display window that does not just replicate the projected view shown in the sandbox itself, but that can show the captured 3D topography from arbitrary points of view, in full 3D, as explained in this video.

To create a secondary view, you first need to edit the `SARndbox` application's configuration file and instruct it to open a second window on a different display. Run in a terminal window:

```sh
xed ~/.config/Vrui-8.0/Applications/SARndbox.cfg
At the beginning of the "Desktop" section, insert the following setting:
section Vrui
    section Desktop
        # Open a second window:
        windowNames += (Window2)
        ...
    endsection
endsection
Then, after the existing "Window" section, insert a new section "Window2" with the following settings:
section Vrui
    section Desktop
        ...
        section Window
            ...
        endsection

        section Window2
            viewerName Viewer
            screenName Screen
            windowType Mono

            # Open the window on a specific video output port:
            outputName <port name>

            # Open the window to full-screen mode:
            windowSize (800, 600)
            windowFullscreen true
        endsection
        ...
    endsection
endsection
```

where `<port name>` is replaced with the name of the actual video output port to which your non-sandbox display is connected, as gathered from your `xrandr` report in [Step 6](#step-6-use-multiple-screens).

After the above changes, `SARndbox` will open a second window on the secondary display, but it will still show the same fixed projector view as the main projector, possibly squished due to a different aspect ratio on the secondary display. To allow an independent view on the secondary display, you have to modify the command line in `SARndbox`. The best way to do this is to edit the `RunSARndbox.sh` shell script you created in [Step 4](#step-4-create-a-desktop-icon-to-launch-the-arsandbox). In a terminal window, run:

```sh
xed ~/src/SARndbox-2.8/RunSARndbox.sh
```

The simple command line entered in [Step 4](#step-4-create-a-desktop-icon-to-launch-the-arsandbox) only has two arguments:

```sh
./bin/SARndbox -uhm -fpv
```

These instruct `SARndbox`, in order, to apply the default elevation color map, and to lock the display to the projector calibration matrix captured in [Step 6 in "System Integration, Configuration, and Calibration"](system_integration.md#step-6-projector-or-camera-calibration). By default, `SARndbox` applies display options from the command line to all windows it opens, but this can be overridden by using a `-wi` `<window index>` argument, where `<window index>` is the zero-based index of a window. After seeing that argument, `SARndbox` will apply all following display options to all windows with the same or a larger index, and also reset the `-fpv` option. Thus, append the following to the command line of `SARndbox`:

```sh
./bin/SARndbox -uhm -fpv -wi 1 -rws
```

The (optional) `-rws` argument will draw water as a three-dimensional surface instead of applying it to the topography as a texture, as is the default setting. This will create a better representation of water flow when seen from oblique angles.

After these changes, the ARSandbox will show a navigable three-dimensional view of the 3D topography model, including water drawn as a real 3D surface, on the secondary display. The secondary display can be rotated, translated, and scaled as in many other 3D graphics software, using a combination of mouse buttons and keyboard keys. Most importantly, pressing the left mouse button will rotate the view around the center of the window, and pressing the ++z++ key while moving the mouse will translate the view in the window's plane. Pressing the left mouse button and ++z++ key together, while moving the mouse up and down, will scale the view (zoom in or out), as will rolling the mouse wheel. Finally, pressing the left mouse button, the ++z++ key, and the ++left-shift++ key, while moving the mouse up or down, will "dolly" the view in or out of the window's plane, as will rolling the mouse wheel while pressing the ++left-shift++ key.

On start-up, `SARndbox` will show a default view of the 3D topography in its secondary window. If you would like to show a different view on start-up, you can use the navigation techniques described in the previous paragraph to create the perfect view that you want, and then save it. To do this, press and hold the right mouse button to show the main application menu. Move the mouse to the last entry, "Vrui System", and from there to the "View" sub-menu, then to the "Save View..." entry, and there release the right mouse button. This will open a file selection dialog where you can specify a file name and location for the view file about to be written. Leave the default file name ("SavedViewpoint0001.view") and default location (the ARSandbox's project directory) alone, and press the "OK" button. This will close the file selection dialog and save the current view to the selected file.

After exiting from `SARndbox`, change the command line in the `RunSARndbox.sh` shell script again, this time appending a command to load the view file you just saved. Run in a terminal:

```sh
xed ~/src/SARndbox-2.8/RunSARndbox.sh
```

and change the command line to:

```sh
./bin/SARndbox -uhm -fpv -wi 1 -rws -loadView
SavedViewpoint0001.view
```

If you changed the name of the view file before saving it (or renamed it later), use the correct name here. Afterwards, the ARSandbox will start up with your preferred view displayed in its secondary window.