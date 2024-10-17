# aws-iot-esp32-low-power-sample
在IoT领域,有非常多需要低功耗的应用场景,比如:智能家居的门窗检测, 水电表的自动抄表,智慧农业的土壤,环境监测. 这些场景都需要保证设备在低功耗的情况下,能够保证长时间稳定的和服务端通信.
所以,我们设计这个低功耗的IoT连接方案. 此方案主要的思路是,将IoT Client端SDK跑在一些低功耗且成本相对较低的MCU芯片上. 将一些处理要求较轻的任务,比如上报数据,接收服务端指令等工作从设备的CPU剥离出来,达到尽量减少唤醒CPU的目标,以降低功耗.

此Demo主要展示如下功能：
- 定时唤醒,接收服务端信息
- 通过GPIO0来模拟设备上报事件
- 利用ESP32的Deep Sleep模式降低功耗
  
乐鑫科技的ESP32芯片有非常良好的功耗管理设计,并且对FreeRT OS提供了完善的支持,可以非常方面的接入AWS IoT Core服务, 故本此方案的设备端采用ESP32-EYE 开发板进行.

![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/esp32-eye-devkit.png)

在deep sleep模式下的理论功耗,见下表:
![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/deepsleep-power.png)

## 系统架构图
![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/architecture.png)

设备端外挂了一个ESP32的MCU完成wifi连接,以及与IoT Core服务通信功能,尽量降级维持mqtt通信的功耗.

## 验证工程
### 安装乐鑫ESP-IDF开发工具链
请参考[官方配置步骤](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html), 工具链版本选择tag: v4.4.6 .

### 在IoT Core 服务创建新的Thing
由于是功能验证阶段,所以权限设置的较为宽松, policy配置如下.
```
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish",
        "iot:Receive",
        "iot:PublishRetain"
      ],
      "Resource": "arn:aws:iot:us-east-1:<account id>:topic/*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Subscribe",
      "Resource": "arn:aws:iot:us-east-1:<account id>:topicfilter/*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:us-east-1:<account id>:client/ESP32*"
    }
  ]
}
```

### 将证书和key拷贝到工程目录
```
git clone https://github.com/heqiqi/aws-iot-esp32-low-power-sample.git
```
拷贝证书:
```
cp <CA cert> aws-iot-esp32-low-power-sample/main/certs/root_cert_auth.crt
cp <device priv key> aws-iot-esp32-low-power-sample/main/certs/client.key
cp <device cert> aws-iot-esp32-low-power-sample/main/certs/client.crt
```

### 配置IoT endpoint 和 Wifi ssid
进入 esp-idf 目录,export编译环境变量
```
 . ./export.sh
```
启动menuconfig工具:
```
idf.py menuconfig
```

EndPoint 配置如下:
![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/iot-endpoint.png)

Wifi 配置如下:
![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/wifi-ssid.png)

### 编译设备固件

编译
```
idf.py build
```
烧录&monitor
```
idf.py flash monitor
```

### 设备端流程图
![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/ESP32-workflow.png)

设备流程介绍:
- 当ESP32首次上电点时,首先初始化flash,PSRAM的外围设备
- 按照配置连接Wifi
- 同时监听IO, 此DEMO坚挺的事GPIO15,实际生产中可以时GPIO,I2S等任何IO
- Wifi连接成功后,通过mqtt 客户端连接iot core服务获取服务端消息
- 如果有消息进入消息队列
- 根据消息队列里的消息类型,执行设备控制或消息上报等操作
- 消息队列消息完成,即刻进入低功耗的deep sleep 模式
- 指导timer唤起重新获取服务端消息,或被监听的GPIO事件唤起


### 设备端日志
被timer从deep sleep模式唤醒 --> 检查消息队列 --> pub to IoT --> 再进入deep sleep
![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/running-log.png)

### 功耗对比
正常工作模式,电流为120mA
![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/high-current.png)

进入低功耗的Deep Sleep 模式, 工作电流为 10mA
![image](https://github.com/heqiqi/aws-iot-esp32-low-power-sample/blob/main/img/low-power-current.png)


## 结论
通过将IoT SDK的代码移植到功耗较小的MCU上,并且使用定期上报和事件唤醒的方式,可以有效的控制IoT设备功耗,增加使用时长.
从工作电流来比较,功耗仅为长期保持连接的10%左右.