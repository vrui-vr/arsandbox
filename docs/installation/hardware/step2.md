# Step 2: Internally calibrate the Kinect camera (optional)

## Background on Kinect camera calibration

Individual Kinect cameras have slightly different internal layouts and slightly different optical properties, meaning that their internal calibrations, i.e., the projection matrices defining how to project depth images back out into 3D space and how to project color images onto those reprojected depth images, differ individually as well. While all Kinects are factory-calibrated and contain the necessary calibration data in their firmware, the format of those data is proprietary and cannot be read by the Kinect 3D video capture project software, meaning that each Kinect camera has to be calibrated internally before it can be used. In practice, the differences are small and a Kinect camera can be used without internal calibration by assigning default calibration values, but it is strongly recommended to perform calibration on each device individually.

The internal calibration procedure requires a semi-transparent calibration target; precisely, a checkerboard with alternating clear and opaque tiles. Such a target can be constructed by gluing a large sheet of paper to a clear glass plate, drawing or ideally printing a checkerboard onto it, and cutting out all "odd" tiles using large rulers and a sharp knife. It is important that the tiles are lined up precisely and have precise sizes, and that the clear tiles are completely clean without any dust, specks, or fingerprints. Calibration targets can have a range of sizes and numbers of tiles, but we found the ideal target to contain 7 $\times$ 5 tiles of 3.5in $\times$ 3.5in each.

Given an appropriate calibration target, the calibration process is performed using RawKinectViewer and its `Draw Grids` tool. The procedure is to show the calibration target to the Kinect camera from a variety of angles and distances, and to capture a calibration tie point for each viewpoint by fitting a grid to the target's images in the depth and color streams interactively.

## Detailed calibration procedure

1. Aim Kinect camera at calibration target from a certain position and angle. It is important to include several views where the calibration target is seen at an angle.

2. Capture an average depth frame by selecting the "Average Frames" main menu item, and wait until a static depth frame is displayed.

3. Drag the virtual grids displayed in the depth and color frames using the `Draw Grid` tool's first button until the virtual grids exactly match the calibration target. Matching the target in the depth frame is relatively tricky due to the inherent fuzziness of the Kinect's depth camera. Doing this properly will probably take some practice. The important idea is to get a "best fit" between the calibration target and the grid. For the particular purpose of the Augmented Reality Sandbox, the color frame grids can be completely ignored because only the depth camera is used; however, since calibration files are shared between all uses of the Kinect 3D video capture project, it is best to perform a full, depth and color, calibration.

4. Press the `Draw Grid` tool's second button to store the just-created calibration tie point.

5. Deselect the "Average Frames" main menu entry, and repeat from step 1 until a sufficient number of calibration tie points have been captured. The set of all tie points already selected can be displayed by pressing the `Draw Grid` tool's third button.

    > After all tie points have been collected:

6. Press the `Draw Grid` tool's fourth button to calculate the Kinect camera's internal calibration parameters. These will be written to an intrinsic parameter file in the Kinect 3D video capture project's configuration directory.

## Tutorial video

This calibration step is illustrated in the following tutorial video:

<iframe width="1000" height="400" src="https://www.youtube-nocookie.com/embed/Qo05LVxdlfo?si=ibidI_s-lWqEcqse" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>
