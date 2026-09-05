# CSPS 服务器电源接入 Home Assistant（ESPHome external_components 方案）

> **AI 修改声明**：本项目基于原 [hitsword/csps_esphome](https://github.com/hitsword/csps_esphome)
> 项目，全部修改（按 ESPHome 文档推荐的方式改写为 external_components、风扇
> 转速控制、能耗统计、本 README 等）均由大语言模型 **GLM-5.3-Flash**（Z.ai /
> 智谱）完成；其中风扇转速范围的标定数据来自真机实测。

将 KCORES CSPS（华为、HP、联想等服务器拆机电源）通过 ESP8266/ESP32 接入 Home
Assistant：读取电源的输入/输出电压、电流、功率、温度、风扇转速与转化效率，
控制电源启动信号与风扇转速，并累计用电量接入 HA 能源面板。

> **注意**：本项目不是 ESPHome 官方发布的项目。"ESPHome 官方"仅用于描述所
> 引用的组件机制与文档出处；ESPHome 文档推荐的自定义硬件做法是编写
> external component（可放本地目录，也可发布为 git 仓库），本项目即按此
> 方式实现。

## 版本沿革

| 版本 | 方案 |
| --- | --- |
| 第一版（原作者 [hitsword/csps_esphome](https://github.com/hitsword/csps_esphome)） | 使用 ESPHome 的 `custom` platform，Arduino 风格的 `KCORES_CSPS` 库 + `Wire` 直接操作 |
| 第二版 | ESPHome 自 2023.12 移除所有 `custom` 平台后，借助第三方仓库 `robertklep/esphome-custom-component` 恢复 `custom` 平台继续使用（该方式不受 ESPHome 官方支持） |
| 第三版（本仓库） | 按 ESPHome 文档推荐的方式改写为 [external_components](https://esphome.io/components/external_components) 标准外部组件（`local` 本地组件，带 Python 配置校验与代码生成），C++ 侧改用 ESPHome `i2c::I2CDevice` 接口，与官方组件同一套框架（总线管理、错误码、状态上报），并新增风扇转速控制、能耗统计等功能 |

## 目录结构

```
output/
├── csps-power.yaml            # ESP8266（NodeMCU）配置，与原配置引脚一致，可直接替换
├── csps-power-esp32.yaml      # ESP32（esp32dev）配置
├── secrets.yaml.example       # WiFi 密钥模板（复制为 secrets.yaml 并填入实际值）
├── LICENSE                    # GPL-3.0 许可证
├── my_components/             # 外部组件目录（YAML 通过 external_components 加载）
│   └── csps/
│       ├── __init__.py        # 组件主配置：I2C 地址、更新间隔、校验和开关
│       ├── sensor.py          # sensor 平台：电压/电流/功率/温度/风扇/效率
│       ├── text_sensor.py     # text_sensor 平台：出厂信息（制造商、型号等）
│       ├── number.py          # number 平台：风扇目标转速
│       ├── button.py          # button 平台：恢复风扇自动控制
│       ├── csps.h             # C++ 组件实现
│       └── csps.cpp
└── README.md
```

## 快速开始

1. 把整个 `output` 文件夹的内容放到你的 ESPHome 配置目录（或直接在该目录编辑）。
2. 复制 `secrets.yaml.example` 为 `secrets.yaml`，填入**你自己生成**的以下内容
   （仓库中不含任何真实密钥，也不要使用示例占位值）：
   - `wifi_ssid` / `wifi_password`：WiFi 信息
   - `api_encryption_key`：Home Assistant API 加密密钥（32 字节 base64）。
     用 ESPHome 仪表盘新建设备时会自动生成一段，直接复制；或执行
     `openssl rand -base64 32`
   - `ota_password`：OTA 升级密码（32 位十六进制），执行 `openssl rand -hex 16` 生成
   - `ap_password`：WiFi 失败时备用热点的密码（至少 8 位）
3. 按硬件选择配置文件（ESP8266 用 `csps-power.yaml`，ESP32 用
   `csps-power-esp32.yaml`），确认 i2c 引脚与控制引脚和实际接线一致，
   并按电源型号修改 `substitutions` 中的 `rated_current`（用于负载率）。
   IP 地址默认使用 **DHCP 自动获取**；如需固定 IP，取消配置中 `manual_ip`
   段的注释并修改。
4. 正常编译烧录即可，例如：

```bash
esphome run csps-power.yaml
```

## 实体一览

**传感器（sensor 平台 `csps`，寄存器换算与原版一致）**

| 实体 | 说明 |
| --- | --- |
| `Fan_Speed` | 风扇实际转速（寄存器 0x1E），测试风扇控制时的"真实反馈" |
| `Temp 1` / `Temp 2` | 内部温度（0x1A / 0x1C） |
| `Voltage_In` / `Current_In` / `Power_In` | 输入侧（墙插）电压/电流/功率 |
| `Voltage_Out` / `Current_Out` / `Power_Out` | 输出侧（12V）电压/电流/功率 |
| `Efficiency` | 转化效率 = Power_Out / Power_In |
| `Output_Power`（默认注释） | 电源直接上报的输出功率（0x12，整数） |

**统计（官方组件，纯配置实现）**

| 实体 | 说明 |
| --- | --- |
| `Total_Energy_Out` / `Total_Energy_In` | 累计用电量 kWh（开机起累计）。接入 HA 能源面板：**电费选 `Total_Energy_In`（墙插侧）**；`total_increasing` 类型，重启归零不影响能源面板累计 |
| `Daily_Energy_Out` / `Daily_Energy_In` | 今日用电，每天零点（Asia/Shanghai）归零 |
| `Load_Ratio` | 负载率 = Current_Out / 额定电流（`substitutions` 里按电源型号改 `rated_current`） |

**控制**

| 实体 | 说明 |
| --- | --- |
| `Forced Switch` / `Power Switch` | 电源启动信号（官方 gpio switch，与原版一致） |
| `Fan_Speed_Target` | 风扇目标转速（RPM），写入 PMBus 寄存器 0x40。PS-2751-7H 实测有效范围 3600–17300 |
| `Fan_Auto` 按钮 | 向 0x40 写入 0，恢复电源自动风扇控制（PS-2751-7H 实测确认生效） |

**信息（text_sensor 平台，可选）**

`Manufacturer`（制造商）、`Model Name`（型号），以及默认注释着的
`Spare Part Number`（备件号）、`Manufacture Date`（生产日期）、
`Option Kit Number`（套件号）、`CT Date Codes`（CT 日期码）。

## 组件配置说明

### `csps`（主组件）

```yaml
csps:
  - id: psu
    update_interval: 1s      # 轮询间隔，默认 1s（与原版一致）
    address: 0x5F            # PMBus 地址，默认 0x5F
    rom_address: 0x57        # 信息 ROM 地址，默认 address - 8
    verify_checksum: true    # 校验读回数据的校验和，默认开启
```

`verify_checksum: false` 时按原库默认行为只读 2 字节、不校验（不推荐）。

### `number` / `button`（风扇控制）

```yaml
number:
  - platform: csps
    csps_id: psu
    fan_speed:
      name: "Fan_Speed_Target"
      min_value: 3600        # 按实测标定，其他型号参考下节指南重测
      max_value: 17300
      step: 100

button:
  - platform: csps
    csps_id: psu
    fan_auto:
      name: "Fan_Auto"
```

## 风扇控制标定记录与实测指南

寄存器 0x40 的写协议来自 KCORES 原库（`setFanRPM`），无公开文档，以下行为
需真机实测。

**PS-2751-7H（光宝 750W）实测标定结果**

| 项目 | 结果 |
| --- | --- |
| 有效设定下限 | 3600 RPM（实际反馈稳定在 3720–3750 RPM） |
| 有效设定上限 | 17300 RPM（实际反馈稳定在 17460–17535 RPM） |
| 写 0 恢复自动 | ✅ 确认生效，转速回到随负载/温度变化的自动模式 |
| 带载自动覆盖 | 未测（暂无带载条件） |

实际反馈值比设定值高约 100–250 RPM，属电源测速/驱动偏差，不影响控制。
组件内 `min_value` / `max_value` 已按此范围标定。

⚠️ 带载覆盖行为尚未验证：在确认"电源过热时会自动拉高风扇"之前，
**不要在大负载下长期使用偏低的转速设定**，不确定时按 `Fan_Auto` 回到
自动控制。

**其他型号电源的实测指南**

每步之间等 30 秒左右让转速稳定，用非关键负载（假负载或测试机），全程盯着
`Fan_Speed`（实际转速）和 `Temp 1`：

1. **记基线**：记录空载时 `Fan_Speed` 的自动值。
2. **确认可控**：`Fan_Speed_Target` 设 4000，看 `Fan_Speed` 是否跟上；
   没反应就把日志级别改 DEBUG 排查。
3. **探下限**：每次降 500，记下不再跟随时最后能跟上的值。
4. **探上限**：每次加 1000–2000 往上，记下能跟上的最高值。
5. **测自动恢复**：按 `Fan_Auto`（写 0），看是否回到自动模式。
6. **测保护覆盖**：设低转速并加负载，看温度上升时电源是否自动拉高风扇。
   若温度持续上升而风扇不响应，立即按 `Fan_Auto`。

测完把"有效下限 / 有效上限 / 写 0 是否恢复自动 / 带载覆盖"四个结果
填进 YAML 的 `min_value` / `max_value` 即可。

## 与原版的主要差异

1. **不再依赖第三方 shim 仓库**，`external_components` 使用本地目录加载，
   也可把 `my_components/csps` 发布为 git 仓库后改用 `type: git` 加载。
2. **读数失败不再发布 0**：原版读取失败会发布 0（HA 历史曲线出现尖峰），
   现在失败时保留上次数值、记录 WARNING，并自动重试 3 次。
3. **读回数据默认带校验和验证**（三字节之和 mod 256 为 0），可发现传输错误；
   如与实际硬件不兼容，可设置 `verify_checksum: false` 恢复原行为。
4. **效率计算增加除零保护**，空载时不再产生无效值。
5. **出厂信息从日志升级为可选的 text_sensor 实体**；新增风扇转速控制
   （原库有协议、原 ESPHome 配置从未接入）。
6. 新增 `temp2` / `current_in` / `output_power` 三个原库支持但未开放的传感器。
7. 日志级别从 `NONE` 恢复为默认 `INFO`；web_server 在现代版本上重新默认开启
   （原注释"一定要关闭web，不然时不时死机"是老版本 ESPHome 的问题，如仍出现
   可整段删除 web_server 配置）。
8. `preferences: flash_write_interval: 10min` 降低 ESP8266 Flash 磨损
   （"今日用电"断电后最多丢 10 分钟计数；累计用电量靠 HA 能源面板跨重启累计）。

## 硬件参考

- 电路图与 PCB：[oshwhub 电源工程](https://oshwhub.com/ikalyes/dian-yuan-gong-cheng)
- KCORES CSPS 转 ATX 转接板：<https://github.com/KCORES/KCORES-CSPS-to-ATX-Converter>
- 光宝 PS-2751-7H（750W）规格书：<https://github.com/Xing-Fax/CSPC-To-ATX/blob/main/CSPS%20To%20AXT%20Power/manual/PS-2751-7H-LF%20%E5%85%89%E5%AE%9D750W%E7%99%BD%E9%87%91%E7%94%B5%E6%BA%90%E8%A7%84%E6%A0%BC%E4%B9%A6V.D.pdf>
- ESPHome 组件开发文档：<https://esphome.io/contributions/>

## 致谢与许可

- `KCORES_CSPS` 协议实现：AlphaArea（[KCORES-CSPS-to-ATX-Converter](https://github.com/KCORES/KCORES-CSPS-to-ATX-Converter)），GPL 协议
- 原 ESPHome 集成：[hitsword/csps_esphome](https://github.com/hitsword/csps_esphome)
- 本项目全部修改由大语言模型 **GLM-5.3-Flash**（Z.ai / 智谱）完成

本项目的组件代码衍生自上述 GPL 项目，以 **GPL-3.0** 协议发布（见
[LICENSE](LICENSE) 文件）。
