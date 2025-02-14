# LightMessenger

**[LAB401](https://lab401.com)'sLightMessenger** by **[tixlegeek](https://cyberpunk.company)** is an additional hardware module designed for the **[Flipper Zero](https://flipperzero.one/)**. It allows users to display messages and images in the air using [POV](https://en.wikipedia.org/wiki/Persistence_of_vision) on an array yof RGB Leds. With a resolution of 16 pixels in height, users can program text or bitmap images and display them with a simple hand movement.

This module offers convenient functionality for those who want to customize their Flipper Zero with a POV light display. Its integration with the Flipper Zero is straightforward, and it is designed for easy use, providing an additional option for users looking to extend the capabilities of their device.

![lm02](./READMEassets/device.png)

## How it works?

This modules embeds an i2c **accelerometer**(LIS2XX12), and an array of **16 RGB Leds** (SK6805). The accelerometer detects every steps of a swipe motion, and allows the flipper Zero to flash the RGB led array at tight timing, resulting as persistence-of-vision based display.

## How to use

Plug the device on the GPIO header of the flipper zero, then download an install the latest firmware's version on your flipper zero. 
Once everything is installed, navigate to **/Apps/GPIO/401LightMessenger** to launch the application.

![lm02](./READMEassets/lm01.png)

![lm02](./READMEassets/lm02.png)

Press any key to enter the main menu. From here, you can set and display text, draw images, and configure the module.



### Text

Entering the "**Text**" application, you're prompted with a text-input, allowing you to set your own custom text.

![text](./READMEassets/doc_text.png)

Select "save" to save your text. The module should flash once, and the text "Swipe" appears on the screen. 

### Bitmap

Entering the "**Bitmap**" application, you're invited to select a bitmap file to be displayed. 

![lm_bmp02](./READMEassets/doc_bitmap.png)



Only 1bpp bitmaps, with a hight of 16px can be displayed. Once selected, the text "Swipe" appears on the screen. 



### Bitmap Editor

Entering "**BitmapEditor**", you have two options: Create a new doodle, or edit one you've already created/imported. 

![lm_bmp02](./READMEassets/doc_bitmap_edit.png)

#### New bitmap

When selecting "New", you shall enter the width of the doodle you want to draw. 

![lm_bmp02](./READMEassets/lm_bmpedit03.png)

Choose the desired width by using left and right arrows, then press OK.
You'll then have to choose a name for the new file. 

![lm_bmp02](./READMEassets/lm_bmpedit04.png)

When done, select "**save**". To draw, just move the cursor using the arrows, and toggle pixels by pressing the "**Ok**" button. To save, press "**Return**"

![lm_bmp02](./READMEassets/lm_bmpedit05.png)

#### Edit bitmap

The bitmap editor allows you to modify any bitmap files that meet the app's requirement.

### FlashLight

With this amount of ultra-bright leds, it would have been sad to not propose a nice flashlight mode. You can access it by selecting the "**FlashLight**" mode.
 ![lm_bmp02](./READMEassets/doc_flashlight.png)

### Configuration

You can customize some aspects of the LightMessenger by entering "**Configuration**".


![lm_bmp02](./READMEassets/lm_config01.png)

#### Orientation

The orientation allows to choose between "Wheel Up" and "WheelDown". Select the one you're the most comfortable with.

#### Color

Multiple color shaders are available: 

- Single colors (R3d, 0r4ng3, Y3ll0w, Gr33n, Cy4n, Blu3, Purpl3) 
- Animated (Ny4nC4t, R4inb0w, Sp4rkl3, V4p0rw4v3)

## Swipe

![lm_bmp02](./READMEassets/lm_swipe.png)

When "**Swipe!**" shows up, things get serious. It's time to grab **firmly** your flipper zero, and wave it in the air. **Be aware of people/stuff around you!**
The display is based on [PoV](https://en.wikipedia.org/wiki/Persistence_of_vision), so it's effect shows best in a dark/mood lighting condition. It may take a bit to really master the correct swipe motion

> [!TIP]
>
> It's not about speed, it's about consistency!



You can also photobomb if you build up enought skill. 

![lm02](./READMEassets/doc01.png)

## Troubleshooting

### It doesn't work?

If you're having trouble getting the LightMessenger to work, try this:

1. Disconnect the LightMessenger from the Flipper Zero.
2. Restart the Flipper Zero by holding **Back + Left** for 3 seconds.
3. Reconnect the LightMessenger and launch the application again.

### It works, but it's not reliable

The LightMessenger relies on physical interaction and may take some practice to master.

You don’t need to be fast—just focus on making **consistent** motions while keeping the LightMessenger in a vertical position.

During the first three swipes, the LightMessenger calibrates your movements, so no light will be emitted during this phase.

### My arm hurts now

Remember, it's not about speed—it's about consistency. Take your time, experiment, and practice until swiping feels natural.


## Contributing



## Developpement