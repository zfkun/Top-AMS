# Top-AMS
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?logo=c%2B%2B&logoColor=white)
![VSCode](https://img.shields.io/badge/IDE-VSCode-007ACC?logo=visual-studio-code&logoColor=white)
![ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-green?logo=espressif&logoColor=white)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/nccrrv/Top-AMS)
![GitHub License](https://img.shields.io/github/license/nccrrv/Top-AMS)
## 简介
- 本项目还在施工中
- 写一些介绍
- 目前支持最多八个通道
## 指南
首先需要准备以下硬件
### 主控模块
- [合宙esp32C3](https://wiki.luatos.com/chips/esp32c3/board.html)
- PCB版(如果你只需要双色,也可以考虑直接在面包板上接线)
- 电机芯片
<br>之后放上PCB版的嘉立创链接以及电机芯片的具体型号 

- | 通道 | 前向GPIO | 后向GPIO | 备注 |
  |:-------:|:-------|:-------|:-------:|
  | 通道1 | GPIO2 | GPIO3 |
  | 通道2 | GPIO10 | GPIO6 |
  | 通道3 | GPIO5 | GPIO4 |
  | 通道4 | GPIO8 | GPIO9 |
  | 通道5 | GPIO0 | GPIO1 |
  | 通道6 | GPIO20 | GPIO21 |
  | 通道7 | GPIO12 | GPIO13 | 和LED灯冲突 |
  | 通道8 | GPIO18 | GPIO19 | 和USB冲突 |
### 上下料模块
- 上下料模块有两种电机方案,这两种方案在与主控的接线上没有不同
- [N20电机方案](hard\N20电机方案\README.md)
- [TT电机方案](hard\TT电机方案\README.md)

### 刷入固件
- [固件刷入教程](https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/flash_download_tool.html)
### esp配网
- 使用微信小程序 **一键配网**
- 配网协议选择 **SmartConfig** 
- 填入Wifi信息配网
### 连接打印机MQTT
- 在路由器中查看esp32的ip,登入esp32的web管理页面
- MQTT密码为打印机局域网模式里的密码,局域网模式开关不影响连接,建议直接在机器的小屏幕上查看
- 设备序列号除了在机器上直接查看外,也可以在 BambuStudio-设备-固件更新-序列号 中查看
### 配置打印机gcode
- 将对应机型的Gcode加入到打印机换色Gocde前
  - 目前只有A1mini的,但是其他打印机原理上也完全通用,只用改下几个数字就好,欢迎加群测试  
- 打印机使用热床温度范围 **1~17** 与AMS传递通道信息,请避免设置这个范围内的热床温度
- 退料前的回抽参数会自动读取使用的耗材配置,可在耗材配置内更改
- 切片软件的冲刷体积配置也能正常生效
- 冲刷体积一部分流量会被用于进料,请自行测试合适的冲刷体积

## 其他  
### 兼容性测试
- A1 mini 固件版本1.04.00
### 讨论
- Q群:104103478九,注明来意

### 代办
- 完成通道管理页面
- 自动续料
    - 思路:默认会续当前通道的下一个通道的料,在切片时候就要注意好.软件架构上,维护一个长度为使用的通道数(通过是否是NC脚判断)的布尔向量,记忆当前通道是否被续料过,这个记忆状态会在本次打印任务结束后重置
- 异常处理
  - mqtt断开
  - 因为料线刚好没了,无法退线的的报错
- 合入缓冲微动
  
