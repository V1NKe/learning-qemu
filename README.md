## Real World

- produce
  - [CVE-2019-6788](https://github.com/V1NKe/learning-qemu/tree/master/real_world/cve-2019-6788) (about slirp handle TCP/IP heap overflow)
  - [CVE-2020-14364](https://github.com/V1NKe/learning-qemu/tree/master/real_world/CVE-2020-14364) (USB core out of bounds read and write)
  - [TianfuCup2020-QEMU-Error-Handling-Bug](https://github.com/V1NKe/learning-qemu/tree/master/real_world/ErrorHandlingBug) (nvme device uninitialized variable and uninitialized free)
  - [vitio-2.6.0](https://github.com/V1NKe/learning-qemu/tree/master/real_world/some-vuln-examples/virtio-2.6.0) (null pointer reference)
  - [vga-2.6.0](https://github.com/V1NKe/learning-qemu/tree/master/real_world/some-vuln-examples/vga-2.6.0) (out of bounds read and write)
  - [pcnet-2.2.0](https://github.com/V1NKe/learning-qemu/tree/master/real_world/some-vuln-examples/pcnet-2.2.0) (out of bounds read and write)

## Document

- Device Specification
  - [USB](https://github.com/V1NKe/learning-qemu/tree/master/doc/specification/usb)
    - EHCI (ehci specifiction)
    - xHCI (xhci specifiction)
    - UHCI (uhci specifiction)
  - [TCP/IP](https://github.com/V1NKe/learning-qemu/tree/master/doc/specification/tcp-ip)
    - Overview of TCP/IP (relation to qemu-slirp)
  - [ATA/ATAPI](https://github.com/V1NKe/learning-qemu/tree/master/doc/specification/ata-atapi)
    - ATA/ATAPI-5
    - ATAPI-Removable-Rewritable-Specification
    - Working-Draft-ATA/ATAPI-Command-Set-3
    - ATA-Packet-Interface-for-CD-ROMs
  - [NVMe](https://github.com/V1NKe/learning-qemu/tree/master/doc/specification/nvme)
    - A-NVMe-Storage-Virtualization-Sloution

- fuzz
  - [usbfuzz](https://github.com/V1NKe/learning-qemu/tree/master/doc/fuzz/usbfuzz)
    - fuzz usb drivers by device emulation
  - [AFLfuzz](https://github.com/V1NKe/learning-qemu/tree/master/doc/fuzz/aflfuzz)
    - 《When-Virtualization-Encounters-AFL-A-Portable-Virtual-Device-Fuzzing-Framework-With-AFL》

- exploit
  - [虚拟化安全之QEMU与KVM](https://github.com/V1NKe/learning-qemu/tree/master/doc/exploit/%E8%99%9A%E6%8B%9F%E5%8C%96%E5%AE%89%E5%85%A8%E4%B9%8BQEMU%E4%B8%8EKVM)
    - QEMU (relate to [this](https://github.com/V1NKe/learning-qemu/tree/master/real_world/some-vuln-examples))
    - KVM
  - [Misuse-Error-Handling-Leading-To-QEMU-KVM-Escape](https://github.com/V1NKe/learning-qemu/tree/master/doc/exploit/)

- kernel driver
  - [kernel-driver-development](https://github.com/V1NKe/learning-qemu/tree/master/doc/kernel-driver/kernel-driver-development)
    - Linux设备驱动
    - Linux设备驱动开发详解
  - [usb-driver](https://github.com/V1NKe/learning-qemu/tree/master/doc/kernel-driver/usb-driver)
    - USB设备驱动

## Vedio/Web URL

- fuzz
  - Libfuzzer
    - [Virtual Device Fuzzing Support in QEMU](https://www.youtube.com/watch?v=x3GSBKY2oE0)
    - [Fuzzing Virtual Devices in Cloud Hypervisors](https://www.youtube.com/watch?v=Qh7WBxsk3KM)
  - QTest
    - [QTest Device Emulation Testing Framework](https://qemu.readthedocs.io/en/latest/devel/qtest.html)
    - [Qtest Driver Framework](https://qemu.readthedocs.io/en/latest/devel/qgraph.html#qgraph)
- exploit
  - [Scavenger: Misuse Error Handling Leading to Qemu/KVM Escape](https://blackhat.app.swapcard.com/event/black-hat-asia-2021/planning/UGxhbm5pbmdfMzU3MDI0)

## ctf

- done
  - [2017-Blizzard-strng](https://github.com/V1NKe/learning-qemu/blob/master/ctf/done/2017Blizzard-strng.tar.gz)
  - [2018-Defcon-Quals-EC3](https://github.com/V1NKe/learning-qemu/tree/master/ctf/done/2018Defcon-EC3-quals)
  - [2019-qwb-quals-qwct](https://github.com/V1NKe/learning-qemu/tree/master/ctf/done/2019qwb-qwct-quals)
  - [2019-qwb-final-ExecChrome](https://github.com/V1NKe/learning-qemu/tree/master/ctf/done/2019qwb-ExecChrome-final)
  - [2021-RealWorld-quals-Easy_Escape](https://github.com/V1NKe/learning-qemu/tree/master/ctf/done/2021RealWorld-Easy_Escape-quals)
- original
  - [2020-GeekPwn-Quals-Vimu](https://github.com/V1NKe/learning-qemu/tree/master/ctf/original/2020-geekpwn-Vimu-quals)
  - [2020-GeekPwn-Quals-Kemu（2020-N1CTF-Kemu）](https://github.com/V1NKe/learning-qemu/tree/master/ctf/original/2020-geekpwn-Kemu-quals)
  - [2020-GeekPwn-Final-V1NKe's QEMU](https://github.com/V1NKe/learning-qemu/tree/master/ctf/original/2020-geekpwn-V1NKe'sQEMU-final)