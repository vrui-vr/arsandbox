# Step 4: Measure the base plane equation of the sand surface

## Measuring the base plane

Because the Kinect camera can be aimed at the sand surface arbitrarily, the Augmented Reality Sandbox needs to know the equation of the "base plane" corresponding to the average flattened sand surface and the "up direction" defining elevation (+/-) relative to that base plane.

The base plane can be measured using `RawKinectViewer` and the `Extract Planes` tool.

1. Flatten and average the sand surface such that it is exactly horizontal, or place a flat board above the sand surface.
2. Then, capture an average depth frame by selecting the "Average Frames" main menu entry and wait until the depth image stabilizes.
3. Now, use the `Extract Planes` tool to draw a rectangle in the depth frame that *only* contains the flattened sand surface. After releasing the `Extract Planes` tool's button, the tool will calculate the equation of the plane best fitting the selected depth pixels, and print two versions of that plane equation to the terminal: the equation in depth image space, and the equation in camera space. Of these, only the second is important. The tool prints the camera-space plane equation in the form

$$
x * (\text{normal}_x, \text{normal}_y, \text{normal}_z) = \text{offset}
$$

This equation must be entered into the sandbox layout file, which is by default called `BoxLayout.txt` and contained in the Augmented Reality Sandbox's configuration directory. The format of this file is simple; the first line contains the sandbox's base plane equation in the form:

$$
(\text{normal}_x, \text{normal}_y, \text{normal}_z), \text{offset}
$$

The plane equation printed by the `Extract Planes` tool only needs to be modified slightly when pasting it into the sandbox layout file: the "$x\,*$" part has to be removed and the equal sign has to be replaced by a comma. The other four lines in the sandbox layout file are filled in in the next calibration step.

## Base plane and zero elevation level

The base plane equation defines the zero elevation level of the sand surface. Since standard color maps equate zero elevation with sea level, and due to practical reasons, the base plane is often measured above the flattened average sand surface, it might be desirable to lower the zero elevation level. This can be done easily be editing the sandbox layout file.

The zero elevation level can be shifted upwards by increasing the offset value (the fourth component) of the plane equation, and can be shifted downwards by decreasing the offset value. The offset value is measured in cm; therefore, adding 10 to the original offset value will move sea level 10 cm upwards.

## Tutorial video

This calibration step is illustrated in the following tutorial video:

<iframe width="1000" height="400" src="https://www.youtube-nocookie.com/embed/9Lt4J_BErs0?si=x8fMwqAVyiHet4Kp" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>