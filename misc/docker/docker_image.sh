#!/bin/sh
set -e

IMAGE=pottendo-pi1541-builder
START=$SECONDS

echo "=== Building Docker image: ${IMAGE} ==="
docker build -t ${IMAGE} .

ELAPSED=$((SECONDS - START))
echo ""
echo "=== Image built successfully in $((ELAPSED / 60))m $((ELAPSED % 60))s. Run docker_build.sh to build Pi1541. ==="
