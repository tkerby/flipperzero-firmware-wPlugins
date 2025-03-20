# Chief Cooker
Your ultimate Flipper Zero restaurant pager tool. Be a _real chief_ of all the restaurants on the food court!

This app supports receiving, decoding, editing and sending restaurant pager signals. 

Developed for Momentum firmware. 

## Disclaimer
I've built this app for research and learning purposes. But please, don't use it in a way that could hurt anyone or anything.

Use it responsibly, okay?

## Features
- Receive signals from pager stations
- Automatically decode them and dsiplay station number, pager number and action
- Manually change encoding in real-time if automatically detected encoding is not working
- Resend captured message to specific pager to all at once to make them all ring!
- Modify captured signal, e.g. change pager number or action
- Save signals from captured stations and give it a name
- Dsiplay signals from saved stations by it's name or hide them from list
- Send signals from saved stations at any time, no need to capture it again
- Of course, suports working with external CC1101 module to cover the area of all the pagers on your food court! 

## Supported protocols
- Princeton
- SMC5326

## Supported pager encodings
- Retekess TD157
- Retekess TD165/T119
- Retekess TD74
- L8R / Retekess T111 (not tested)
- L8S / iBells ZJ-68 (check [source code](app/pager/decoder/L8SDecoder.hpp#L8) for description)

## Contributing
If you want to add any new pager encoding, please feel free to create PR with it!

Also you can open issue and share with me any captured data (and at least pager number) if you have any and maybe I'll try create a decoder for it

## Building
TODO

## Special Thanks
- [meoker/pagger](https://github.com/meoker/pagger) for Retekess pager encodings
- This [awesome repository](https://dev.xcjs.com/r0073dl053r/flipper-playground/-/tree/main/Sub-GHz/Restaurant_Pagers?ref_type=heads) for Retekess T111 and iBells ZJ-68 files
