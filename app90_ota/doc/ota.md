# OTA方案
MCUboot + 双分区（A/B）+ 串口

# 流程

设备运行时：
1. 从串口接收新固件
2. 写入 Slot1
3. 设置升级标志
4. 重启
5. MCUboot完成镜像切换
6. 新固件确认升级成功


# Flash空间划分

| 区域              | 大小  |
| ----------------- | ----- |
| MCUboot           | 128KB |
| Slot0（当前固件） | 960KB |
| Slot1（升级固件） | 960KB    |


# overlay 
```dts 
&flash0 {

    partitions {
        compatible = "fixed-partitions";

        boot_partition: partition@0 {
            label = "mcuboot";
            reg = <0x08000000 0x20000>;    //128KB
        };

        slot0_partition: partition@20000 {
            label = "image-0";
            reg = <0x08020000 0xF0000>;
        };

        slot1_partition: partition@110000 {
            label = "image-1";
            reg = <0x08110000 0xF0000>;
        };
    };
};

/
{
    chosen {
        zephyr,code-partition = &slot0_partition;
    };
};
```
