# DX-BT311 主从角色与工程配置说明

## 1. 适用范围

本说明对应 MSPM0G3507_M0_Base 工程的 DX-BT311 服务层。当前工程将 BLE 模块接到 UART2；实际引脚、复用和波特率以以下文件为准：

- Config/empty.syscfg
- Config/ti_msp_dl_config.h
- Config/ti_msp_dl_config.c
- BSP/Peripherals/bsp_ble_uart.c
- Config/project_config.h

如果派生工程把 BLE 改接到 UART1，必须同步修改 SysConfig 生成的实例映射和 BLE BSP，不能只改宏名。

## 2. 角色与启动策略

所有产品级选项集中在 Config/project_config.h：

    #define PRJ_BLE_MODULE             (1U)  /* 1=DX-BT311，0=JDY-23 兼容实现 */
    #define PRJ_BLE_ROLE_SLAVE         (0U)
    #define PRJ_BLE_ROLE_MASTER        (1U)
    #define PRJ_BLE_ROLE               PRJ_BLE_ROLE_SLAVE
    #define PRJ_BLE_AUTO_INIT          (1U)
    #define PRJ_BLE_AUTO_PROBE         (1U)
    #define PRJ_BLE_AUTO_CONNECT       (0U)
    #define PRJ_BLE_APPLY_AT_CONFIG    (0U)

首次调试推荐 AUTO_CONNECT=0、APPLY_AT_CONFIG=0。确认串口、模块响应和角色后，再临时打开需要的选项。

> 软件中的 PRJ_BLE_ROLE 只是固件流程选择；当 PRJ_BLE_APPLY_AT_CONFIG 为 0 时，不会改变模块内部保存的 ROLE。

## 3. 从机模式配置

从机用于被手机或另一台主机搜索和连接：

    #define PRJ_BLE_ROLE             PRJ_BLE_ROLE_SLAVE
    #define PRJ_BLE_APPLY_AT_CONFIG  (1U)  /* 只在需要写入模块时临时开启 */
    #define PRJ_BLE_SLAVE_NAME       "M0_BASE_A"
    #define PRJ_BLE_SLAVE_UUID       ""
    #define PRJ_BLE_SLAVE_CHAR       ""
    #define PRJ_BLE_SLAVE_WRITE      ""
    #define PRJ_BLE_SLAVE_NOTI       "1"
    #define PRJ_BLE_SLAVE_ADVI       ""
    #define PRJ_BLE_SLAVE_CLOSEADV   "0"

字段：SLAVE_NAME 是设备名；SLAVE_UUID 是服务 UUID；SLAVE_CHAR 是特征 UUID；SLAVE_WRITE 是可写特征 UUID；SLAVE_NOTI 是通知开关；SLAVE_ADVI 是广播参数；SLAVE_CLOSEADV 是广播开关。具体取值以 DX-BT311 手册为准，不确定的字段留空。

应用顺序为 ROLE -> NAME -> UUID -> CHAR -> WRITE -> NOTI -> ADVI -> CLOSEADV。任一项失败会停止后续写入并记录错误。

## 4. 主机按 MAC 连接

MAC 连接最可靠，不依赖搜索名称：

    #define PRJ_BLE_ROLE              PRJ_BLE_ROLE_MASTER
    #define PRJ_BLE_AUTO_CONNECT      (1U)
    #define PRJ_BLE_MASTER_TARGET_MAC "112233AABBCC"
    #define PRJ_BLE_MASTER_TARGET_NAME ""
    #define PRJ_BLE_MASTER_MUUID      ""

启动流程：初始化 BLE UART -> 可选 AT 探测 -> 执行 AT+CONA<MAC> -> 记录连接状态。MAC 使用模块返回的完整 12 位十六进制字符串，不要带冒号、空格或 0x。

## 5. 主机按名称搜索连接

MAC 为空时可以按名称搜索：

    #define PRJ_BLE_ROLE               PRJ_BLE_ROLE_MASTER
    #define PRJ_BLE_AUTO_CONNECT       (1U)
    #define PRJ_BLE_MASTER_TARGET_MAC  ""
    #define PRJ_BLE_MASTER_TARGET_NAME "M0_BASE_B"
    #define PRJ_BLE_MASTER_MUUID       ""

流程为 AT+INQ 搜索，随后对名称做完整字符串匹配，找到后使用设备序号执行 AT+CONN<n>。名称匹配区分大小写；同名设备较多时请改用 MAC。

## 6. 持久化配置与复位

主要接口：

    dx_bt311_status_t app_dx_ble_apply_project_config(void);
    dx_bt311_status_t app_dx_ble_connect_project_target(void);
    void app_dx_ble_service_task(void *param);

app_dx_ble_apply_project_config() 只写入非空配置，记录是否需要复位，并且不会自动发送 AT+RESET。若日志提示需要复位：

1. 手动复位或重新给模块上电；
2. 将 PRJ_BLE_APPLY_AT_CONFIG 恢复为 0U；
3. 再开启 PRJ_BLE_AUTO_CONNECT 验证连接。

为了避免使用尚未生效的角色和参数，配置写入后若标记为需要复位，自动连接会跳过并打印原因。

## 7. 故障定位

- probe 超时：检查 UART2 TX/RX 是否交叉、是否共地、是否为 9600 8N1、供电是否正常。
- 主机连接失败：确认模块实际 ROLE 已保存为主机；只改固件宏而不应用 AT 配置，模块可能仍是从机。
- 名称找不到：先用 MAC 验证，或检查名称大小写和搜索结果。
- 配置成功但行为未变：按日志提示复位模块。
- BLE 任务创建失败：系统仍继续运行，检查 FreeRTOS 堆空间和任务栈大小。

## 8. 回滚

本次修改前备份：

    bak_fix/before_ble_role_config_20260727_211500

编码修复备份：

    bak_fix/before_ble_role_comment_fix_20260727_220000

只恢复 BLE 相关文件：Config/project_config.h、Application/app_dx_ble_service.*、Application/app_main.c、BSP/Communication/dx_bt311.h。不要对整个工程执行 git reset 或 git clean，因为工程中还有其他未提交改动。
