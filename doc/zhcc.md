
## Ulpoading an FPGA image to Zurich Heterogenous Cloud (ZHC2)
curl --form "binfile=@fw_2018_1016_1603_noSDRAM_ADKU3_25.bin" --form username="" --form password=""="" http://cloudsynthesismaster:9080/sindri/api/testimage

## Deploying a POWER-based CAPI-enabled instance to Zurich Heterogenous Cloud (ZHC2)
curl --form "filename=fw_2018_1016_1603_noSDRAM_ADKU3_25.bin" --form username=a"" --form password="" http://cloudsynthesismaster:9080/sindri/api/testimage

## Connect in the new docker image
ssh opuser@10.12.1.135 (assuming an openVPN tunneling)

# Steps to do on the new VM to run hls_blstm on FPGA
sudo apt-get update
sudo apt-get install g++
cd ~
git clone https://github.com/open-power/snap.git
git clone https://github.com/oprecomp/HLS_BLSTM.git ./snap/actions/hls_blstm
cd snap
make snap_config
cd software
make
cd ../actions/hls_blstm/sw
SNAP_CONFIG=FPGA make
sudo chmod 777 /dev/cxl/afu0.0*
../../../software/tools/snap_maint -vvv
SNAP_CONFIG=FPGA ./snap_blstm -i ../data/samples_1/ -g ../data/gt_1/
