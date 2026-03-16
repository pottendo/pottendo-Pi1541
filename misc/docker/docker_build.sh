#!/bin/sh
set -e

IMAGE=pottendo-pi1541-builder
OUTPUT="$(dirname "$0")/../Pi-Bootpart"
ARCH="$1"
START=$SECONDS

echo "=== Syncing source into image ==="
docker build -t ${IMAGE} .

echo ""
echo "=== Copying Pi-Bootpart base (ROMs, firmware, boot files) to ${OUTPUT} ==="
mkdir -p "${OUTPUT}"
docker run --rm -v "${OUTPUT}:/output" --entrypoint sh ${IMAGE} -c "cp -r /build/Pi-Bootpart/. /output/"

echo ""
echo "=== Building Pi1541 ${ARCH} ==="
if [ -z "${ARCH}" ]; then
    docker run --rm -v "${OUTPUT}:/build/Pi-Bootpart" ${IMAGE}
else
    docker run --rm -v "${OUTPUT}:/build/Pi-Bootpart" ${IMAGE} -a ${ARCH}
fi

ELAPSED=$((SECONDS - START))
echo ""
echo "=== Done in $((ELAPSED / 60))m $((ELAPSED % 60))s. Files in ${OUTPUT} ==="
ls -l "${OUTPUT}"
