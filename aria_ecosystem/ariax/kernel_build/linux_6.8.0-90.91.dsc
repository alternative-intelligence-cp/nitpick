-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA512

Format: 1.0
Source: linux
Binary: linux-headers-6.8.0-90, linux-tools-6.8.0-90, linux-cloud-tools-6.8.0-90, linux-libc-dev, linux-tools-common, linux-cloud-tools-common, linux-tools-host, linux-source-6.8.0, linux-doc, linux-bpf-dev, linux-image-unsigned-6.8.0-90-generic, linux-modules-6.8.0-90-generic, linux-modules-extra-6.8.0-90-generic, linux-headers-6.8.0-90-generic, linux-lib-rust-6.8.0-90-generic, linux-image-unsigned-6.8.0-90-generic-dbgsym, linux-tools-6.8.0-90-generic, linux-cloud-tools-6.8.0-90-generic, linux-buildinfo-6.8.0-90-generic, linux-modules-ipu6-6.8.0-90-generic, linux-modules-iwlwifi-6.8.0-90-generic, linux-modules-usbio-6.8.0-90-generic, linux-image-unsigned-6.8.0-90-generic-64k, linux-modules-6.8.0-90-generic-64k, linux-modules-extra-6.8.0-90-generic-64k, linux-headers-6.8.0-90-generic-64k, linux-lib-rust-6.8.0-90-generic-64k, linux-image-unsigned-6.8.0-90-generic-64k-dbgsym, linux-tools-6.8.0-90-generic-64k, linux-cloud-tools-6.8.0-90-generic-64k,
 linux-buildinfo-6.8.0-90-generic-64k, linux-modules-ipu6-6.8.0-90-generic-64k, linux-modules-iwlwifi-6.8.0-90-generic-64k, linux-modules-usbio-6.8.0-90-generic-64k, linux-image-unsigned-6.8.0-90-generic-lpae, linux-modules-6.8.0-90-generic-lpae, linux-modules-extra-6.8.0-90-generic-lpae, linux-headers-6.8.0-90-generic-lpae, linux-lib-rust-6.8.0-90-generic-lpae, linux-image-unsigned-6.8.0-90-generic-lpae-dbgsym, linux-tools-6.8.0-90-generic-lpae, linux-cloud-tools-6.8.0-90-generic-lpae, linux-buildinfo-6.8.0-90-generic-lpae, linux-modules-ipu6-6.8.0-90-generic-lpae, linux-modules-iwlwifi-6.8.0-90-generic-lpae,
 linux-modules-usbio-6.8.0-90-generic-lpae
Architecture: all amd64 armhf arm64 ppc64el s390x i386 riscv64
Version: 6.8.0-90.91
Maintainer: Ubuntu Kernel Team <kernel-team@lists.ubuntu.com>
Standards-Version: 3.9.4.0
Vcs-Git: git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/noble
Testsuite: autopkgtest
Testsuite-Triggers: @builddeps@, build-essential, fakeroot, gcc-multilib, gdb, git, python3
Build-Depends: gcc-13, gcc-13-aarch64-linux-gnu [arm64] <cross>, gcc-13-arm-linux-gnueabihf [armhf] <cross>, gcc-13-powerpc64le-linux-gnu [ppc64el] <cross>, gcc-13-riscv64-linux-gnu [riscv64] <cross>, gcc-13-s390x-linux-gnu [s390x] <cross>, gcc-13-x86-64-linux-gnu [amd64] <cross>, autoconf <!stage1>, automake <!stage1>, bc <!stage1>, bindgen-0.65 [amd64 arm64 armhf ppc64el riscv64 s390x], bison <!stage1>, clang-18 [amd64 arm64 armhf ppc64el riscv64 s390x], cpio, curl <!stage1>, debhelper-compat (= 10), default-jdk-headless <!stage1>, dkms <!stage1>, flex <!stage1>, gawk <!stage1>, java-common <!stage1>, kmod <!stage1>, libaudit-dev <!stage1>, libcap-dev <!stage1>, libdw-dev <!stage1>, libelf-dev <!stage1>, libiberty-dev <!stage1>, liblzma-dev <!stage1>, libnewt-dev <!stage1>, libnuma-dev [amd64 arm64 ppc64el s390x] <!stage1>, libpci-dev <!stage1>, libssl-dev <!stage1>, libstdc++-dev, libtool <!stage1>, libtraceevent-dev [amd64 arm64 armhf ppc64el s390x riscv64] <!stage1>, libtracefs-dev [amd64 arm64 armhf ppc64el s390x riscv64] <!stage1>, libudev-dev <!stage1>, libunwind8-dev [amd64 arm64 armhf ppc64el] <!stage1>, makedumpfile [amd64] <!stage1>, openssl <!stage1>, pahole [amd64 arm64 armhf ppc64el s390x riscv64] | dwarves (>= 1.21) [amd64 arm64 armhf ppc64el s390x riscv64] <!stage1>, pkg-config <!stage1>, python3 <!stage1>, python3-dev <!stage1>, python3-setuptools <!stage1>, rsync [!i386] <!stage1>, rust-src [amd64 arm64 armhf ppc64el riscv64 s390x], rustc [amd64 arm64 armhf ppc64el riscv64 s390x], rustfmt [amd64 arm64 armhf ppc64el riscv64 s390x], uuid-dev <!stage1>, zstd <!stage1>
Build-Depends-Indep: asciidoc <!stage1>, bzip2 <!stage1>, python3-docutils <!stage1>, sharutils <!stage1>, xmlto <!stage1>
Package-List:
 linux-bpf-dev deb devel optional arch=amd64,armhf,arm64,i386,ppc64el,riscv64,s390x
 linux-buildinfo-6.8.0-90-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-buildinfo-6.8.0-90-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-buildinfo-6.8.0-90-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-cloud-tools-6.8.0-90 deb devel optional arch=amd64,armhf profile=!stage1
 linux-cloud-tools-6.8.0-90-generic deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-cloud-tools-6.8.0-90-generic-64k deb devel optional arch=arm64 profile=!stage1
 linux-cloud-tools-6.8.0-90-generic-lpae deb devel optional arch=armhf profile=!stage1
 linux-cloud-tools-common deb kernel optional arch=all profile=!stage1
 linux-doc deb doc optional arch=all profile=!stage1
 linux-headers-6.8.0-90 deb devel optional arch=all profile=!stage1
 linux-headers-6.8.0-90-generic deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-headers-6.8.0-90-generic-64k deb devel optional arch=arm64 profile=!stage1
 linux-headers-6.8.0-90-generic-lpae deb devel optional arch=armhf profile=!stage1
 linux-image-unsigned-6.8.0-90-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-image-unsigned-6.8.0-90-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-image-unsigned-6.8.0-90-generic-64k-dbgsym deb devel optional arch=arm64 profile=!stage1
 linux-image-unsigned-6.8.0-90-generic-dbgsym deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-image-unsigned-6.8.0-90-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-image-unsigned-6.8.0-90-generic-lpae-dbgsym deb devel optional arch=armhf profile=!stage1
 linux-lib-rust-6.8.0-90-generic deb devel optional arch=amd64 profile=!stage1
 linux-lib-rust-6.8.0-90-generic-64k deb devel optional arch=amd64 profile=!stage1
 linux-lib-rust-6.8.0-90-generic-lpae deb devel optional arch=amd64 profile=!stage1
 linux-libc-dev deb devel optional arch=amd64,armhf,arm64,i386,ppc64el,riscv64,s390x
 linux-modules-6.8.0-90-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-6.8.0-90-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-6.8.0-90-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-modules-extra-6.8.0-90-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-extra-6.8.0-90-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-extra-6.8.0-90-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-modules-ipu6-6.8.0-90-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-ipu6-6.8.0-90-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-ipu6-6.8.0-90-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-modules-iwlwifi-6.8.0-90-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-iwlwifi-6.8.0-90-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-iwlwifi-6.8.0-90-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-modules-usbio-6.8.0-90-generic deb kernel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-modules-usbio-6.8.0-90-generic-64k deb kernel optional arch=arm64 profile=!stage1
 linux-modules-usbio-6.8.0-90-generic-lpae deb kernel optional arch=armhf profile=!stage1
 linux-source-6.8.0 deb devel optional arch=all profile=!stage1
 linux-tools-6.8.0-90 deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-tools-6.8.0-90-generic deb devel optional arch=amd64,armhf,arm64,ppc64el,s390x profile=!stage1
 linux-tools-6.8.0-90-generic-64k deb devel optional arch=arm64 profile=!stage1
 linux-tools-6.8.0-90-generic-lpae deb devel optional arch=armhf profile=!stage1
 linux-tools-common deb kernel optional arch=all profile=!stage1
 linux-tools-host deb kernel optional arch=all profile=!stage1
Checksums-Sha1:
 27dce9d91afd9433620628fde58a686f5e53cebb 230060117 linux_6.8.0.orig.tar.gz
 e581cc25f354a5d5ce2e1ab9e1cd3fa20db71555 6075998 linux_6.8.0-90.91.diff.gz
Checksums-Sha256:
 26512115972bdf017a4ac826cc7d3e9b0ba397d4f85cd330e4e4ff54c78061c8 230060117 linux_6.8.0.orig.tar.gz
 0b97ab3372b7521a306c2fc8fc70280846ea25c49eb8f360d839f9a3f8f45eb3 6075998 linux_6.8.0-90.91.diff.gz
Files:
 3ce3b99b065c7c0507b82ad0f330e2fd 230060117 linux_6.8.0.orig.tar.gz
 4d49e97e295b38c71309eec5127c7513 6075998 linux_6.8.0-90.91.diff.gz
Ubuntu-Compatible-Signing: ubuntu/4 pro/3

-----BEGIN PGP SIGNATURE-----

iQJRBAEBCgA7FiEEfiwIK8Bxx+qw8pSyuvL3+R/lFUsFAmkcafIdHG1hbnVlbC5k
aWV3YWxkQGNhbm9uaWNhbC5jb20ACgkQuvL3+R/lFUsE8Q//c2256fHQt9GUs4RH
YMladKnL5mRuP1TkZXdBAIQrYNmXZZZRbTgk/XA9PUV5jdh10z4bUxARXPvMFqt/
qJVyRCqeEvNEZ93S3NGgSI9+811Nveob02DQAPbgy8cU3vWqBmdNq6eLW49NIs38
mCI36Vtu3+mqBDo9/cTXTwR1MhJnRa2n4VeFzGfOTY23TOfOFIWHBa+NeyH1sqMP
evs1s+7zKo3FM+n8buaJHKWfdzficLd7kUTJ11e5nAJ43dtNjbCDqB3QtV6TH6zx
KvC1L8quomSPdo6eVp9biw1xd5MGJEJpjRF8Muc+8sAwztZQGAh+b3nnZLN6134z
BUQbVFxzjGCXfIBUV8bk7r4+3phHItgtcGmPQ0D1JvrUWjWy06T9SAgLjqIy/3jh
FxVXDVT5Kp1eFn80c8xnSVWqMdRQbkOlHqWEM7aQ7pSyjxiLMuNMboxqzcdj5Q3D
23+VSnq04oYOW2vRhcthya35IRW9jSeG9CUqYQ4J/X+K8oejLTQc+SHoh2hcoCCe
PzDrXA8HlLcKZOdEqJGGah2bn5mEjiw9ms0z+FoBngNM+trnApf0ARDukky+rja2
241tF26LFCWWiCDwyX5wol3ZRxSqdqF+CFla/Q+FKZsRweJvQmxq1wv1HVLdykcQ
tXJ+hODWiyl/vY5qkda8e0nk15Q=
=4tdL
-----END PGP SIGNATURE-----
