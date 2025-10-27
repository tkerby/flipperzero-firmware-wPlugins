v1.1
released app
📝 List of Changes (Brief)
Added the ability to select the measurement channel: ALS or WHITE.

Modified the sensor reading to read the register appropriate for the selected channel.

Added a side scrollbar (a slider showing the position) in the settings menu.

Limited the settings view to a maximum of 2 items simultaneously with scrolling.

Aligned and offset the values in the settings menu (I2C address, gain, channel) for better text and value positioning.

Fixed the scrollbar position calculation to prevent it from going outside the frame.

The unit "lx" for the main value is now displayed below the value (clearer, in a separate font).

Removed the key legend from the main screen and improved the general screen layout.

Various fixes for I2C stability, input handling, and sensor initialization.
