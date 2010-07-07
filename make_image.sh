dd if=/dev/zero of=fat16_image_blank.img bs=1M count=100
mkdosfs -s 32 -F 16 fat16_image_blank.img
