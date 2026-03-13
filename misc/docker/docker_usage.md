# Docker Build Usage

## Quick Start

Two pairs of scripts are provided — use `.bat` on Windows, `.sh` on Linux/Mac.

| Purpose | Windows | Linux/Mac |
|---------|---------|-----------|
| Build Docker image (once) | `docker_image.bat` | `./docker_image.sh` |
| Build Pi1541 | `docker_build.bat` | `./docker_build.sh` |

On Linux/Mac, make scripts executable first:
```sh
chmod +x docker_image.sh docker_build.sh
```

---

## Step 1 — Build the Docker image

Run once. Repeat only if the `Dockerfile` changes.

```bat
docker_image.bat
```
```sh
./docker_image.sh
```

This installs all build tools, downloads the ARM cross-compilers, clones and patches circle-stdlib, and fetches ROMs and firmware. Expect ~10 min on first run. Result is a ~6 GB image stored locally by Docker.

---

## Step 2 — Build Pi1541

Run every time you want to build. Optionally pass a target architecture.

```bat
docker_build.bat                   ← all architectures (pi3-32, pi3-64, pi4-32, pi4-64)
docker_build.bat pi4-64            ← single architecture
```
```sh
./docker_build.sh
./docker_build.sh pi4-64
```

Kernel images and all boot files (ROMs, firmware, config) are written to `..\Pi-Bootpart` on the host. Copy the contents of that directory to your Pi1541 SD card boot partition.

---

## Other Useful Commands

### Inspect the image interactively
```bat
docker run --rm -it --entrypoint bash pottendo-pi1541-builder
```

### Force a full image rebuild (ignores cache)
```bat
docker build --no-cache -t pottendo-pi1541-builder .
```

### Check image size
```bat
docker images pottendo-pi1541-builder
```

### Free disk space

Remove the Pi1541 image (~6 GB):
```bat
docker rmi pottendo-pi1541-builder
```

Remove dangling intermediate layers left behind by failed or interrupted builds:
```bat
docker image prune
```


---

## Advanced

### Why source is copied into the image, not mounted

Mounting the source directory and running `dos2unix` inside the container would fix CRLF line endings but would modify the actual files on the host — causing git on Windows to show them as modified. Instead, source is `COPY`'d into the image so the container works on its own copy, leaving host files untouched.

### Why the Pi1541 build runs at `docker run` time, not `docker build` time

`build.sh` always does a full clean rebuild (`make mrproper` + `make clean`) regardless, so there is nothing to cache. Baking the build into the image would just bloat it with artifacts that get thrown away on the next build. Running at `docker run` time also allows passing the target architecture as an argument.

### Layer caching

The Docker image is structured in layers to minimise rebuild time when the `Dockerfile` changes:

```
Layer 1: Base OS + packages                       → stable, never re-runs
Layer 2: ARM toolchains                           → stable, never re-runs
Layer 3: COPY build.sh + patches + config files  → re-runs only when those files change
Layer 4: build.sh -c (clone circle-stdlib,
         fetch ROMs/firmware, build CBM-FileBrowser) → re-runs only when layer 3 changes
Layer 5: COPY full source + .git/                → re-runs on any source change
```

The Pi1541 build itself is not a layer — it runs at `docker run` time.

### `.git/` is included in the image

The full `.git/` directory is copied into the image because `build.sh` uses two git operations:
- `git remote get-url origin` — sanity check that the script is running in the right repo
- `git describe --tags` — generates the version string embedded in the firmware
