# IconEdit

## Icon editor for the FlipperZero

Create images and animated icons up to 128x64 in size directly on the Flipper! Easily preview how your images, icons, and animations will look directly on the device. Send your images to your PC without swapping the SDCard! Significantly improves your development workflow by eliminating the need to copy images to your build folders, rebuild, and re-launching your app after every image edit.

* All in-app icons were created with IconEdit!

## Features

* Pixel level editing
* Draw lines, circles, and rectangles
* Scrollable canvas (if image has a dimension greater than 32px)
* Create animated icons: insert/duplicate and delete frames, change frame rates
* Open **.png** files
* Save your icons as **.png**, **.xbm**, and even in a C source file format for direct use in your code!
  * Files are saved in the **apps_data/iconedit** folder on the SD card
  * Multiple numbered **.png** and **.xbm** files are created for animations
  * Simply copy the saved **.png** files from your SD card to your application's **images** folder to be included in your next **.fap** build
* **Even better**, directly transfer C source file representations of the icons over USB to your IDE! Or use the supplied Python script to receive PNG and BMX files in binary!
* Preview of your icon, including animation playback
  * Play / pause animations
  * Change Zoom level during playback
  * Frame by Frame control

Find out more on the Git repository!