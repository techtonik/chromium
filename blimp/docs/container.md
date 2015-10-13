# Running the engine in a Docker container

For local development and testing, you can run the engine in a Docker
container.

The steps are:

1. Bundle the engine and its dependencies.

1. Build a Docker image.

1. Create a Docker container.


## About Docker

To get a high-level overview of Docker, please read [Understand the
architecture](https://docs.docker.com/introduction/understanding-docker/).
Optional reading includes reference guides for
[`docker run`](https://docs.docker.com/reference/run/) and
[Dockerfile](https://docs.docker.com/reference/builder/).


### Installation

For Googlers running Goobuntu wanting to install Docker, see
[go/installdocker](https://goto.google.com/installdocker). For other
contributors using Ubuntu, see [official Docker
installation instructions](https://docs.docker.com/installation/ubuntulinux/).


## Bundle Engine

The `blimp/engine:bundle_blimp_engine` build target will bundle the engine and
its dependencies into a tarfile, which can be used to build a Docker image.


## Build Docker Image

Using the tarfile you can create a Docker image:

```bash
docker build -t blimp_engine - < ./out-linux/Debug/blimp_engine_deps.tar
```

## Create Docker Container

From the Docker image you can create a Docker container (i.e. run the engine):

```bash
docker run blimp_engine
```

You can also pass additional flags:

```bash
docker run blimp_engine --with-my-flags
```

See the [blimp engine `Dockerfile`](../engine/Dockerfile) to find out what flags
are passed by default.
