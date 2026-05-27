Raspi Setup
-----------

Enable cgroupv1 for timescaledb

sh
```/boot/firamware/cmdline.txt
cgroup_enable=memory
```

Install libcap
```shell
sudo apt install libpcap-dev
```

Allow libcamera
```shell
uv venv --system-site-packages
```

Running
```shell
CONFIG_PATH="./config.toml" uv run fastapi run --host 0.0.0.0
```