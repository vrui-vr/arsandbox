# Step 5: Measure the extents of the sand surface

The Augmented Reality Sandbox needs to know the lateral extents of the visible sand surface with respect to the base plane. These are defined by measuring the 3D positions of the four corners of the flattened average sand surface using `RawKinectViewer` and a 3D measurement tool, and then entering those positions into the sandbox layout file.

Start `RawKinectViewer` and create a 3D measurement tool by assigning a `Measure 3D Positions` tool to some button. Then measure the 3D positions of the four corners of the flattened sand surface in the order lower left, lower right, upper left, upper right; in other words, form a mirrored Z starting in the lower left.

To measure a 3D position, press and release the button to which the measurement tool was bound inside the depth frame. This will query the current depth value at the selected position, project it into 3D camera space, and print the resulting 3D position to the console. Simply paste the four corner positions, in the order mentioned above, into the sandbox layout file.