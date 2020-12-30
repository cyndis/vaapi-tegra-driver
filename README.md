# vaapi-tegra-driver

A VAAPI backend driver for NVIDIA Tegra SoC's, based on multimedia units accessed through Host1x.

Currently supported features:

- NV12 to RGB colorspace conversion
- X11/DRI2 surface presentation

Currently supported SoC's:

- Tegra210
- Tegra186

## Requirements

- C++14-capable compiler
- libva headers
- libdrm headers
- Kernel with VIC driver and CONFIG_DRM_TEGRA_STAGING=y

## Building and testing

Building:

```
mkdir build
cd build
cmake ..
make
```

Testing:

```
export LIBVA_DRIVERS_PATH=<build directory>
export LIBVA_DRIVER_NAME=tegra

cd <libva path>/test/putsurface
./putsurface
```
