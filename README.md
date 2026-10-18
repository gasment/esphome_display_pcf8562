## 一、显示效果预览

- 主控只负责写入屏幕内容，显示由段码屏驱动IC维持
- 主控睡眠后，显示维持，屏幕与驱动待机电流较小，适合低功耗场景
<img width="649" height="468" alt="image" src="https://github.com/user-attachments/assets/40b563a8-fcf2-42cf-94cc-cdfeca2fe21f" />




## 二、硬件选型与电路连接

1. 段码屏选型

   - 目前组件只针对适配此段码屏的图标版本  
     【淘宝】7天无理由退货 https://e.tb.cn/h.jcd21wa2coPvigz?tk=T9LLTud7G9J CZ001 「温湿度计段码LED显示屏LCD液晶屏21脚双排直插温湿度仪表显示屏」  
     点击链接直接打开 或者 淘宝搜索直接打开  
     ​<img width="638" height="498" alt="image" src="https://github.com/user-attachments/assets/9c42e054-1c90-417c-85a0-f5a88e115a20" />
     <img width="639" height="441" alt="image" src="https://github.com/user-attachments/assets/d40afe52-30c7-430d-919b-68005ef0b0ee" />


2. 驱动选型

   - PCF8562TT  
     <img width="238" height="208" alt="image" src="https://github.com/user-attachments/assets/994c4ff1-6bee-40bd-a105-a56bc000b0f5" />

3. 硬件连接

   - 段码屏针脚定义：

     - 正面看10个针脚一边为上，11个针脚一边为下，PIN1为右下角，顺时针递增
     - PIN1-PIN4为COM1-COM4，PIN5-PIN21为SEG0-SEG16
   - PCF8562TT的BP0-BP3按顺序连接到段码屏的PIN1-PIN4，PCF8562TT的S0-S16按顺序连接到段码屏的PIN5-PIN21
   - 示例原理图如下，此处省略I2C上拉电路，自行处理，注意就近放置VDD与VLCD的电容（大胆点，也可以省掉电容）  
     <img width="809" height="753" alt="image" src="https://github.com/user-attachments/assets/14a9e8d4-026f-4763-9d48-ded499145cde" />

‍

‍

## 三、esphome配置

1. 引入外部组件（支持esphome 2026.7.0以上使用esp-idf toolchain编译）

   ```yaml
   external_components:
     - source:
         type: git
         url: https://github.com/gasment/esphome_display_pcf8562
         ref: main
       components: [ pcf8562 ]
       refresh: 24h
   ```

2. 组件主体配置

   ```yaml
   #如已配置i2c组件，使用已有id即可
   i2c:  
     sda: GPIOxx   #自定义IO
     scl: GPIOxx   #自定义IO
     scan: true         
     id: i2c_bus_a   #自定义i2c_id

   display:
     - platform: pcf8562
       id: main_display
       i2c_id: i2c_bus_a
       address: 0x38
       hardware_subaddress: 0
       bias: "1/3"
       restore_from_rtc: true
       update_interval: 1s
   ```

   - 部分配置说明：

     - restore_from_rtc：true/false，设置为true，则将显示内容缓存到RTC,防止深度睡眠唤醒时屏幕内容被清空和刷新，造成视觉断层。
     - update_interval，保留轮询用于重试失败的 I2C 写入，画面不变时不重复发送

3. 设置屏幕显示内容

   - 支持原生action接口对接各种自动化语法：pcf8562.content_set
   - 完整配置示列：

     ```yaml
     on_xxxxx:
       - pcf8562.content_set:
           id: main_display         #pcf8562组件id
           temperature: 20.2        # float：温度，上行显示示列 20.20
           humidity: 56.78          # float：湿度，下行显示示列 56.78
           wifi: true              # bool：Wi-Fi 图标 S6
           bluetooth: false        # bool：蓝牙图标 S5
           battery_outline: true   # bool：电池外框 S9
           battery_left: true      # bool：左电量块 S12
           battery_middle: false   # bool：中电量块 S11
           battery_right: false    # bool：右电量块 S10
           temperature_icon: true  # bool：温度计图标 S3
           humidity_icon: true     # bool：湿度图标 S4
           celsius: true           # bool：摄氏度单位 S8
           percent: true           # bool：百分号 S1
     ```
   - 温湿度动态赋值示列,此处使用SHT30-DIS传感器

     ```yaml
     sensor:
       - platform: shtx_legacy
         id: sht30_main
         address: 0x44
         i2c_id: i2c_bus_a
         update_interval: 5s
         temperature:
           id: sht30_temperature
           accuracy_decimals: 2
           state_class: measurement
           device_class: temperature
           unit_of_measurement: "°C"
           name: "Temperature"
           filters:
             - filter_out: nan
             - delta: 0.1
           on_value:
             then:
               - pcf8562.content_set:
                   id: main_display
                   temperature: !lambda return x;

         humidity:
           id: sht30_humidity
           accuracy_decimals: 2
           state_class: measurement
           device_class: humidity
           unit_of_measurement: "%"
           name: "Humidity"
           filters:
             - filter_out: nan
             - delta: 0.1
           on_value:
             then:
               - pcf8562.content_set:
                   id: main_display
                   humidity: !lambda return x;
     ```
   - 其他用法

     - 主动显示无效状态：  

       ```yaml
        - pcf8562.content_set:
             id: main_display
             temperature: !lambda return NAN;
             humidity: !lambda return NAN;
       ```
     - C++ lambda 调试接口（正常使用 action 时不要添加持续全亮 lambda）  

       ```yaml
       lambda: |-
         it.clear();                    // 清空缓存
         it.all_on();                   // 点亮屏幕全部 68 个显示元素
         it.all_ram_on();               // 全部 128 位 RAM 置 1，用于诊断
         it.show_com(1);                // 仅点亮 COM1，可分别使用 1、2、3、4
         it.set_segment(0, 1, true);     // SEG0 / COM1，数字 1 的 A 段
       ```

## 四、段码显示规则

- 非负温度和湿度默认两位小数，自动处理四舍五入与进位。
- 负温度取整到个位并关闭小数点：-10 显示为「空白/-/1/0」。
- 湿度达到 100（包括进位到 100）时，关闭小数点并右对齐显示 100。
- 无效或超出显示范围的数值显示 ----，关闭本行小数点。
- 更新数值只影响本行数字和小数点，单位与其他图标保持原状态。
- 电池外框和三个电量块独立控制，不自动计算电量等级。
- 冷启动默认两行 ----；深睡唤醒可选恢复 RTC 画面，不清屏。
- 传感器 filter\_out: nan 会阻止该无效值触发 on\_value。

‍

	

‍

##
