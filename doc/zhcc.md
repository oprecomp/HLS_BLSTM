
## Ulpoading an FPGA image to Zurich Heterogenous Cloud (ZHC2)
curl --form "binfile=@fw_2018_1016_1603_noSDRAM_ADKU3_25.bin" --form username="" --form password=""="" http://cloudsynthesismaster:9080/sindri/api/testimage

## Deploying a POWER-based CAPI-enabled instance to Zurich Heterogenous Cloud (ZHC2)
curl --form "filename=fw_2018_1016_1603_noSDRAM_ADKU3_25.bin" --form username=a"" --form password="" http://cloudsynthesismaster:9080/sindri/api/testimage

