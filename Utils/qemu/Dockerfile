# Espressif RISC-V QEMU + esptool, for booting the ESP32-C3 firmware locally.
# Mirrors the CI qemu-smoke job so you can iterate without pushing.
#
# Pinned to amd64 because Espressif ships QEMU as an x86_64 Linux binary; on Apple
# Silicon this runs under Docker's amd64 emulation (works, just slower).
FROM --platform=linux/amd64 ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates wget xz-utils python3 python3-pip \
        libglib2.0-0 libsdl2-2.0-0 libpixman-1-0 libslirp0 libnuma1 \
    && pip3 install --no-cache-dir esptool \
    && rm -rf /var/lib/apt/lists/*

# Latest riscv32 QEMU from Espressif's fork.
RUN wget -q -O /tmp/qemu.tar.xz \
        "$(wget -qO- https://api.github.com/repos/espressif/qemu/releases/latest \
          | grep -o 'https://[^\"]*qemu-riscv32-softmmu[^\"]*x86_64-linux-gnu.tar.xz' \
          | head -n1)" \
    && tar -xf /tmp/qemu.tar.xz -C /opt && rm /tmp/qemu.tar.xz
ENV PATH=/opt/qemu/bin:$PATH

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
