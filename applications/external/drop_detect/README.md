# FliPort

flipper zero imu sample collector for training the motion classifier.
four screens, one per label: drop / idle / walking / fidget.

## install on a flipper

prereqs: python 3.10+, a flipper zero on usb, qFlipper closed.

one-time setup:

```powershell
python -m pip install --upgrade ufbt
ufbt update
```

build + flash from this folder:

```powershell
ufbt launch
```

builds, copies `dist/drop_detect.fap` to the device, and starts it.
on the flipper: `Apps -> Tools -> FliPort`.

if `ufbt launch` can't see the device, just `ufbt` to build, then drop
`dist/drop_detect.fap` into `apps/Tools/` on the sd card via qFlipper.

## how it records

pick a screen, do the motion, and it writes one csv per session to the sd card
under `/ext/apps_data/drop_detect/<label>/`. each file is named by a timestamp.

the format is one header row `order,ax,ay,az,gx,gy,gz` then one row per sample.
it reads the ICM42688P over spi at 100 hz, accel at +/-16 g and gyro at
+/-2000 dps. drop records a fixed clip of 300 samples (about 3 s); the other
three record until you press back.

these csv files are the training data. copy them off the device into
`model/data/drop_detect/<label>/` (or `model/data/user_data/<label>/` to
fine-tune) and train on the pc.

## code layout

- `drop_detect_app.c` sets up the imu and the menu, and starts a capture when
  you enter a recording screen.
- `datarecord.c` is the capture thread: it polls the sensor and writes the csv.
- `driver/ICM42688P.c` is the sensor driver (taken from AirMouse, unchanged).
- `views/` holds the four animated screens, one per label.
