# vaapi-tegra-driver

A VAAPI backend driver for NVIDIA Tegra SoC's, based on multimedia units accessed through Host1x.
This driver is experimental, don't expect it to work in every situation.

Currently supported features:

- NV12 to RGB colorspace conversion
- MPEG2 decoding
- H264 decoding (very experimental, known issues)
- X11/DRI2 surface presentation

Currently supported SoC's:

- Tegra210
- Tegra186
- Tegra194

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

mpv --hwdec=vaapi-copy --hwdec-codecs=all video.m2v
```

## Contributing

The project is licensed under the MIT license. To contribute, you need to add a Signed-off-by
tag to each of your commits, certifying the following:

### Developer’s Certificate of Origin 1.1

By making a contribution to this project, I certify that:

* The contribution was created in whole or in part by me and I have the right to submit it under the open source license indicated in the file; or
* The contribution is based upon previous work that, to the best of my knowledge, is covered under an appropriate open source license and I have the right under that license to submit that work with modifications, whether created in whole or in part by me, under the same open source license (unless I am permitted to submit under a different license), as indicated in the file; or
* The contribution was provided directly to me by some other person who certified (a), (b) or \(c\) and I have not modified it.
* I understand and agree that this project and the contribution are public and that a record of the contribution (including all personal information I submit with it, including my sign-off) is maintained indefinitely and may be redistributed consistent with this project or the open source license(s) involved.

(This is the same process as for the Linux kernel itself.)
