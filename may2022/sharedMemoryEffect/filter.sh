
keyword="LAST"

echo "SHARED"
cat SHARED.log | grep ${keyword} | awk '{print $2}'

echo "Device"
cat DEVICE.log | grep ${keyword} | awk '{print $2}'

echo "Combined Host/Device"
cat COMBINED.log | grep ${keyword} | awk '{print $2}'

echo "HOST_ONLY"
cat HOST_ONLY.log | grep ${keyword} | awk '{print $2}'


