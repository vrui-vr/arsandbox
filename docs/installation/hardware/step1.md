# Step 1: Per-pixel depth correction (optional)

Kinect cameras have non-linear distortions in their depth measurements due to uncorrected lens distortions in the depth camera. The Kinect 3D video capture project has a calibration tool to gather per-pixel correction factors to "straighten out" the depth image.

To calculate depth correction coefficients, start the `RawKinectViewer` utility and create a `Calibrate Depth Lens` tool (See "Using Vrui Applications" in the Vrui HTML documentation on how to create tools).

Then, find a completely flat surface and point the Kinect camera perpendicularly at that surface from a variety of distances. Ensure that the depth camera only sees the flat surface and no other objects and that there are no holes in the depth images.

Now, capture one depth correction tie point for each distance between the Kinect camera and the flat surface:

1. Line up the Kinect camera.

2. Capture an average depth frame by selecting the "Average Frames" main menu item, and wait until a static depth frame is displayed.

3. Create a tie point by pressing the first button bound to the `Calibrate Depth Lens` tool.

4. De-select the "Average Frames" main menu item, and repeat from step 1 until the surface has been captured from sufficiently many distances.

    > After all tie points have been collected:

5. Press the second button bound to the `Calibrate Depth Lens` tool to calculate the per-pixel depth correction factors based on the collected tie points. This will write a depth correction file to the Kinect 3D video capture project's configuration directory, and print a status message to the terminal.