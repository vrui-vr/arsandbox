# Setup and calibration

Before the Augmented Reality Sandbox can be used, the hardware (physical sandbox, Kinect camera, and projector) has to be set up properly, and the various components have to be calibrated internally and with respect to each other. While the sandbox can be run in "trial mode" with very little required setup, for the full effect the following steps have to be performed in order:

1. (*Optional*) Calculate per-pixel depth correction coefficients for the Kinect camera.

2. (*Optional*) Internally calibrate the Kinect camera. We **strongly recommend** skipping this step on initial installation, and only performing it if there are intolerable offsets between the real sand surface in the AR Sandbox and the projected topographic image.

3. Mount the Kinect camera above the sandbox so that it is looking straight down, and can see the entire sand surface. Use `RawKinectViewer` from the Kinect 3D video capture project to line up the depth camera while ignoring the color camera.

4. Measure the base plane equation of the sand surface relative to the Kinect camera's internal coordinate system using `RawKinectViewer`'s plane extraction tool. (See "Using Vrui Applications" in the Vrui HTML documentation on how to use `RawKinectViewer`, and particularly on how to create/destroy tools.)

5. Measure the extents of the sand surface relative to the Kinect camera's internal coordinate system using KinectViewer and a 3D measurement tool.

6. Mount the projector above the sand surface so that it projects its image perpendicularly onto the flattened sand surface, and so that the projector's field-of-projection and the Kinect camera's field-of- view overlap as much as possible. Focus the projector to the flattened average-height sand surface.

7. Calculate a calibration matrix from the Kinect camera's camera space to projector space using the CalibrateProjector utility and a circular calibration target (a CD with a fitting white paper disk glued to one surface).

8. Test the setup by running the Augmented Reality Sandbox application.
