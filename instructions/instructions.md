# Usage instructions

## Your first use
Imagine you are on a food court you want to become chief on. 

First, open the app and select "Scan for station signals". 

The app will start receiving signals and show you once it receives something

<img src="screenshots/main-scan.png" width="256"> <img src="screenshots/scan-empty.png" width="256"> <img src="screenshots/scan-capture1.png" width="256">

Okay, now you received something. Now, let's test if transmission decoded correctly and find the restaurant who sent the transmission!

To do this, click on center button (actions) and select the first one, "Resend to ALL":

<img src="screenshots/scan-capture1-actions.png" width="256"> <img src="screenshots/scan-capture1-resend.png" alt="Description" width="256">

Where are they all running? Is the dinner ready yet? Unfortunately it's not. Just their new chief is learning...

Let's assume that you somehow found out, that it was a restaurant called "Street Food" who sent the signal. Now let's save it's signal to your SD card!

Go back from actions and push the "Edit >" (right arrow) button. Then scroll down to "Save signal as...", give it a name and then create a new category for it. It's convenient to use restaurant name for signal name and mall (or food court/place name where restaurant are located) for the category name to make sure that signals from different places will not mess up. 

<img src="screenshots/scan-capture1.png" width="256"> <img src="screenshots/scan-capture1-edit-save.png" width="256"> <img src="screenshots/scan-capture1-save-name.png" width="256">

<img src="screenshots/scan-capture1-categories.png" width="256"> <img src="screenshots/scan-capture1-category-name.png" width="256"> <img src="screenshots/scan-capture1-categories-with-new.png" width="256">

Congratulations! Your saved your first captured signal and now can use it anytime you want to call someone to the restaurant's food pickup.

But it would be great now to distinguish the signals from "Street Food" from the other ones. And you can do it!

Navigate to "< Config" (left button) and select the category to the newly created one. Then go back.

<img src="screenshots/scan-capture1.png" width="256"> <img src="screenshots/scan-capture1-config.png" width="256"> <img src="screenshots/scan-capture1-conf-select-cat.png" width="256">

<img src="screenshots/scan-capture1-conf-cat-selected.png" width="256"> <img src="screenshots/scan-capture1-with-name.png" width="256">

As you can see, now instead of hex value and station number there is a restaurant name you given to the signal. 

And what's this? A new signal? Yes, but not completely new. Street Food just called pager with another number (7). But your flipper successfully recognized their signal because you already saved one to the current category and showed you it's name. 

<img src="screenshots/scan-capture2.png" width="256">

## Your second use

Now imagine you came to the same food court next day and want to call somebody's pager at the Street Food.

## App's screens explanation

### Scan stations screen

<img src="screenshots/scan-capture1.png" width="256">

The values here are:
- `CBC042` - signal hex code
- `815` - station number (in current encoding)
- `4` - pager number (in current encoding)
- `RING` - action (in current encoding)
- `x8` - number of detected signal repeats, will not show more than `x99`

Note: if you change the signal's encoding in "Edit" menu, station number, pager and action here will also change. 
