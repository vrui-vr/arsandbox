# System Integration, Configuration, and Calibration

<!-- define abbreviations -->
*[ARSandbox]: Augmented Reality Sandbox

## Table of Contents

- [System Integration, Configuration, and Calibration](#system-integration-configuration-and-calibration)
    - [Table of Contents](#table-of-contents)
    - [Step 1: Connect and Configure the 3D Camera](#step-1-connect-and-configure-the-3d-camera)
        - [Step 1a (Optional): Calculate Per-pixel Depth Correction](#step-1a-optional-calculate-per-pixel-depth-correction)
    - [Step 2: Align the 3D Camera](#step-2-align-the-3d-camera)
    - [Step 3: Measure Sandbox's Base Plane Equation](#step-3-measure-sandboxs-base-plane-equation)
    - [Step 4: Measure Sandbox's 3D Box Corner Positions](#step-4-measure-sandboxs-3d-box-corner-positions)
    - [Step 5: Align the Projector](#step-5-align-the-projector)
    - [Step 6: Projector or Camera Calibration](#step-6-projector-or-camera-calibration)


These installation steps connect the additional system components, specifically the 3D camera (Kinect or other camera) and the digital projector, physically align them with the sandbox for optimal image quality, and calibrate the camera with respect to the projector so that real and projected features in the sandbox line up precisely.

??? info "Heads up!"
    Checkout the [full walk-through video](http://youtu.be/R0UyMeJ2pYc) for steps through #7, as well as earlier steps. This video is for an older version of Linux Mint as well as older versions of the Vrui, Kinect, and AR Sandbox packages; in case of any (small) discrepancies between the video and these instructions, ignore the video and follow these instructions. Starting at Step 8, reference the [ARSandbox Calibration video](https://youtu.be/EW2PtRsQQr0?si=dwgqin5M8ekZwcg2). 

## Step 1: Connect and Configure the 3D Camera

Instructions for this step start at [28:52 in the walk-through video](https://youtu.be/R0UyMeJ2pYc?t=28m52s).

Plug in your 3D camera and download intrinsic calibration parameters directly from its firmware. In a terminal window, run:

```sh
sudo /usr/local/bin/KinectUtil getCalib 0
```

This might ask you for your password again; if so, enter it to continue.


## Step 1a (Optional): Calculate Per-pixel Depth Correction

First-generation Kinect cameras (Kinect-for-Xbox-360) have a certain amount of non-linear depth distortion. If you point such a camera at a flat surface, the 3D reconstruction will not be flat, but slightly bowl-shaped. This distortion can prevent getting a good alignment between the physical sand surface and the topographic map projection in Step 10.

RawKinectViewer has a built-in calibration utility to correct for this distortion. This step is not necessary for operation of the AR Sandbox, but it usually improves alignment quality. Again, this issue only applies to first-generation Kinect cameras (models 1414, 1473, and 1517). It is easiest to perform this calibration step with the Kinect camera removed from the AR Sandbox, so if your Kinect is difficult to remove after assembly, it might be a good idea to do this step before putting it in place.

To perform per-pixel depth correction, place your Kinect camera such that it faces a large flat non-shiny surface, like an empty wall. Then start RawKinectViewer as administrator, as it will need to write a calibration file to the system location /usr/local/etc/Vrui-8.0/Kinect-3.10. In a terminal window, run:

```sh
sudo /usr/local/bin/RawKinectViewer -compress 0
```

Enter your password if/when asked.

To begin calibration, move your camera towards the flat surface until black blotches start appearing in the depth image stream on the left side of RawKinectViewer's display window, and then pull back until there are only small and isolated black spots. It is best to have the camera face the flat surface head-on; rotate the camera left/right and up/down until the displayed depth color is uniform between the left, right, top, and bottom edges.

Next, assign a calibration tool to two arbitrary keys. Press the first key, say "A", and move the mouse to highlight "Calibrate Depth Lens" in the tool selection menu that pops up, then release the key. This will close the tool selection menu and open a "Creating Calibrate Depth Lens Tool..." dialog prompting you to press another key for tool function "Calibrate." Pick a key, say "S", press and release it. This will close the dialog, and finish tool assignment.

To capture a calibration tie point, keep the camera very still and press the "A" key once. This will show a "Capturing average depth frame..." message. Do not touch or move the camera while this message is being displayed. After it disappears a few seconds later, move the camera a little distance further away from the flat surface, and press "A" again to capture the next tie point.

Repeat this process a few times to collect anywhere between five and ten tie points, from distances between about 0.5m and 1.5m. Once done, press the "S" key. This will calculate per-pixel depth correction factors and write them to a calibration file in the `/usr/local/etc/Vrui-8.0/Kinect-3.10` directory. RawKinectViewer will print "Writing depth correction file `/usr/local/etc/Vrui-8.0/Kinect-3.10/DepthCorrection-<camera serial number>.dat`" when it is finished. If any error messages appear at this point, close RawKinectViewer and redo the entire process. Otherwise, close RawKinectViewer, install the camera in the AR Sandbox, and continue with the next installation step.

## Step 2: Align the 3D Camera
Align your camera so that its field of view covers the interior of your sandbox. Use RawKinectViewer to guide you during alignment. To start it, run in a terminal window:

```sh
cd ~/src/SARndbox-2.8
RawKinectViewer -compress 0
```

Ignore the color video stream on the right side of RawKinectViewer's display window and focus on the depth image stream on the left (the AR Sandbox does not use the color video stream). Move and/or rotate the camera until it can see the entire interior of your sandbox.

## Step 3: Measure Sandbox's Base Plane Equation

Instructions for this step start at [0:00 in the ARSandbox Calibration video](https://youtu.be/EW2PtRsQQr0?si=O3l4AQc7fXGRddkls).

Calculate your sandbox's base plane by following the beginning of the video linked above. You can use the already-running instance of RawKinectViewer from Step 7.

To recap, you need to bind an "Extract Planes" tool from the root tool menu to some unused mouse button or keyboard key (see the note on "Tool Assignment" in Step 3 for details), then select "Average Frames" from RawKinectViewer's main menu, wait for the depth image display to freeze, and then draw a rectangle on the depth image by pressing and holding the button/key to which you assigned the "Extract Planes" tool. This will print the plane equation best fitting the selected rectangle to the terminal window from which you ran RawKinectViewer. After extracting the base plane, you should turn "Average Frames" back off in RawKinectViewer's main menu.

You need to enter the base plane equation (and the 3D sand surface extents in the next step) into the BoxLayout.txt file in etc/SARndbox-2.8 inside the SARndbox source directory. To edit the file using xed, run in a terminal window:

```sh
cd ~/src/SARndbox-2.8
xed etc/SARndbox-2.8/BoxLayout.txt &
```

The ampersand ("&") at the end of the second command will keep the terminal window usable while the text editor is running. Now enter the base plane equation as described in the video. To copy text from a terminal window, highlight the desired text with the mouse, and then either right-click into the terminal window and select "Copy" from the pop-up menu that appears, or press Shift-Ctrl-c. To paste into the text editor, use the "Edit" menu, or press Ctrl-v. Or, highlight the desired text in the terminal window with the mouse, and then move the mouse into the desired position in the text editor window and press the middle mouse button to copy and paste.

RawKinectViewer prints two plane equations when a depth plane is extracted: the first in depth space, the second in camera space. The AR Sandbox needs the second, camera-space, plane equation. After copying it, the equation has to be reformatted slightly. RawKinectViewer will print:

```sh 
Camera-space plane equation: x * <some vector> = <some offset>
```

where `<some vector>` is a three-component direction vector defining the "up" direction in camera coordinates, typically close to (0.0, 0.0, 1.0), and `<some offset>` is the vertical position of the measured plane underneath the camera, which is in centimeters and negative. BoxLayout.txt needs that plane equation in the following format:
```sh 
<some vector>, <some offset>
```

where the direction vector and the offset are separated by a comma.

??? note
    For some reason, the depth plane equation reported by second-generation Kinect cameras (Kinect-for-Xbox-One) may be inverted. Before continuing, check that the fourth component (offset) of the camera-space plane equation is in fact negative. If it is not, flip the signs of all four components of the plane equation in BoxLayout.txt, e.g., (-0.01, 0.04, -0.9991), 105.3 becomes (0.01, -0.04, 0.9991), -105.3.

## Step 4: Measure Sandbox's 3D Box Corner Positions

Measure the 3D extents of the sand surface. As of version 3.2 of the Kinect package, this can be done inside RawKinectViewer as well by following the instructions in the AR Sandbox calibration video, starting at 4:10. Make sure to measure the box corners in the order lower-left, lower-right, upper-left, upper-right.

To recap, you need to bind a "Measure 3D Positions" tool from the root tool menu to some unused mouse button or keyboard key (see Step 3 for details), and then click on the corners of your sandbox in the depth image using the button/key to which you assigned the "Measure 3D Positions" tool. This will print a sequence of 3D positions to the terminal window from which you ran RawKinectViewer.

After you have copied the box corner positions into the text editor as described in the video, save the file (via the "File" menu or by pressing Ctrl-s), and quit from the text editor (via the "File" menu or by pressing Ctrl-q or by closing the window).

After Steps 8 and 9 have been completed, the contents of BoxLayout.txt should look like the following, with different numbers depending on your installation:

<!-- look into formatting matrix outputs -->
```sh
(-0.0076185, 0.0271708, 0.999602), -98.0000
(  -48.6846899089,   -36.4482382583,   -94.8705084084)
(   48.3653058763,   -34.3990483954,   -89.3884158982)
(   -50.674914634,    35.8072086558,   -97.4082571497)
(   48.7936140481,    36.4780970044,     -91.74159795)
```

Ensure that this text starts in the first column of the first line, and that the file contains no other text at all.

## Step 5: Align the Projector

Align your projector such that its image fills the interior of your sandbox. You can use the calibration grid drawn by Vrui's XBackground utility as a guide. In a terminal window, run:

```sh 
XBackground
```

After the window showing the calibration grid appears, press F11 to toggle it into full-screen mode. Ensure that the window really covers the entire screen, i.e., that there are no title bar, desktop panel, or other decorations left. Press the "Esc" key to close XBackground's window when you're done.

Ensure to disable all digital image distortion features of your projector before alignment, and only use optical features, i.e., optical focus adjustment and optical zoom. Specifically, disable any kind of digital keystone correction, and check that the projector maps the incoming video signal 1:1 to its display pixels. The best way to check for 1:1 matching is to look at the central horizontal bar in XBackground's test image. It should be a clear pattern of alternating white and black one-pixel-wide vertical lines with no smearing or stair steps.

Slight overprojection outside of the sandbox, and any remaining keystone distortion of the projected image, will be taken care of by the following projector calibration step.

## Step 6: Projector or Camera Calibration

!!! warning
    Set CalibrateProjector's window to full-screen mode by pressing F11 before proceeding. 

Instructions for this step start at [10:10 in the ARSandbox Calibration video](https://youtu.be/EW2PtRsQQr0?si=O3l4AQc7fXGRddkl&t=610).

Calibrate the Kinect camera and the projector with respect to each other by running the CalibrateProjector utility:
```sh
cd ~/src/SARndbox-2.8
./bin/CalibrateProjector -s <width> <height>
```
where `<width>` and `<height>` are the width and height of your projector's image in pixels, respectively. For example, for an XGA projector like the recommended BenQ model, the command would be:

```sh
./bin/CalibrateProjector -s 1024 768
```

Ensure that the given image size exactly matches the size of the projector's input video signal.

To recap, you need to bind a "Capture" tool from the root tool menu to two unused mouse buttons or keyboard keys to capture tie points. For example, to bind a "Capture" tool to the "1" and "2" keys, first press and hold "1." This will pop up the tool selection menu. While holding "1," move the mouse to highlight the "Capture" menu entry. Then let go of the "1" key. This will close the tool selection menu, and open a "Creating Capture Tool..." dialog prompting you to press another key for tool function "Capture Background." Now press and release the "2" key. This will close the dialog, and finish tool assignment. Afterwards, press and release "1" to capture a tie point and advance calibration, and press and release "2" to re-capture the background sand surface after you have made any changes to it, such as digging a hole to capture a low tie point.

<!-- move above preceding paragraph and in "warning" ? -->
Please read the preceding paragraph carefully, and follow its instructions precisely. If you get a red screen in response to pressing a key/button during calibration, you are pressing the wrong key/button. The first assigned tool key ("1" in the instructions above) will capture a tie point and advance calibration, i.e., move the calibration crosshairs. The second assigned tool key ("2" in the instructions above) will re-capture the background, indicated by turning the display red for a few seconds, and not advance calibration.

The calibration program expects a disk of diameter 120mm (4.7"), which is a standard CD, CD-ROM, or DVD. The easiest way to create a calibration disk is to glue a sheet of white paper to the data side of a CD/DVD/..., carefully cut around the edge of the CD, and draw a cross onto the paper that intersects exactly in the center of the CD's hole.

??? note
    Do not worry if the projected calibration image (yellow background, yellow or green disk) does not line up with the physical sandbox. This calibration step will make the image line up after it's done.