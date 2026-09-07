# Ti5/Encos Three-Master Runtime Configuration

This directory contains the ENI XML files required by the three-master Ti5/Encos runtime.

- `master1_eni.xml`: runtime ENI configuration for EtherCAT master index 1.
- `master2_eni.xml`: runtime ENI configuration for EtherCAT master index 2.

These files were recovered from the original runtime output directory:

`examples/build/bin/M1.xml`
`examples/build/bin/M2.xml`

They are kept here instead of under `build/bin` so runtime topology is part of the source tree and can be reviewed, copied by CMake, and overridden explicitly.

The ROS transport uses these bundled files by default for master 1 and master 2. Override them with:

```bash
export MOTOR_MASTER1_ENI_FILE=/path/to/master1.xml
export MOTOR_MASTER2_ENI_FILE=/path/to/master2.xml
```

To disable bundled ENI files and fall back to ESI discovery for unspecified masters:

```bash
export MOTOR_USE_BUNDLED_ENI=0
```

The standalone executable keeps ESI behavior by default. Pass `--use-bundled-eni` or explicit `-f1`/`-f2` paths when ENI mode is required.
