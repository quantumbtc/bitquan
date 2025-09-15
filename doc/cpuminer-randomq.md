cpuminer-randomq 使用说明

概述
- cpuminer-randomq 是一个独立的 CPU 矿工，通过 JSON-RPC 与本地 Bitquantum Core 节点交互，采用 RandomQ PoW 算法进行挖矿。
- 支持两种模式：
  - 使用 GBT 返回的整块 hex，仅修改 nonce 后提交（性能更好）。
  - 当 GBT 不提供 hex 时，完全在客户端组装 coinbase 与区块后挖矿并提交（纯客户端 PoW）。

前置条件
- 本地已运行 Bitquantum Core 节点并开启 RPC（默认只监听 127.0.0.1）。
- 节点配置示例（Windows `%LOCALAPPDATA%/Bitquantum/bitquantum.conf`，或使用对应平台路径）：
  
  ```
  server=1
  rpcbind=127.0.0.1
  rpcallowip=127.0.0.1
  ```

编译
- 使用项目现有的 CMake 构建流程，目标二进制位于 `bin/cpuminer-randomq`（Windows 为 `.exe`）。

命令行参数
- 基础 RPC
  - `-rpcconnect=<ip>`：RPC 服务器地址（默认 127.0.0.1）
  - `-rpcport=<port>`：RPC 端口（随链类型而变，可不填）
  - `-rpcuser=<user>`、`-rpcpassword=<pw>`：RPC 账户/密码（建议本地使用 cookie，默认自动读取）
  - `-rpccookiefile=<path>`：RPC cookie 文件路径（不填则从网络对应的 datadir 自动解析）
  - `-rpcwait`、`-rpcwaittimeout=<n>`：等待 RPC 服务就绪（秒），0 表示不超时

- 挖矿与性能
  - `-address=<bech32>`：挖矿奖励接收地址（必填）
  - `-threads=<n>`：挖矿线程数（默认 CPU 逻辑核心数）
  - `-cpucore=<n>`：将线程绑到前 n 个 CPU 核心（0..n-1）
  - `-maxtries=<n>`：每轮最大尝试次数，超过后刷新区块模板（默认 1000000）

快速示例
- Linux/macOS：
  ```bash
  ./cpuminer-randomq -rpcwait -address=btq1...youraddr... -threads=16
  ```

- Windows PowerShell：
  ```powershell
  .\cpuminer-randomq.exe -rpcwait -address btq1...youraddr... -threads 16
  ```

运行时输出
- 每 5 秒打印一次哈希率：
  - Current：最近 5 秒内的瞬时 H/s
  - Average：自启动以来的平均 H/s
  - Total：累计尝试次数

纯客户端 PoW（无 hex 模式）说明
- 当 `getblocktemplate` 未提供 `hex` 时，矿工将：
  1) 使用 `version`、`previousblockhash`、`curtime`、`bits` 构建区块头；
  2) 使用 `height`、`coinbasevalue`、`coinbaseaux.flags` 构建 coinbase，`-address` 指定的地址作为找零输出；
  3) 按 `transactions[].data` 解码并加入所有非 coinbase 交易；
  4) 如返回了 `default_witness_commitment`，为 coinbase 设置 32 字节 witness reserved 值并追加 OP_RETURN 承诺输出；
  5) 重算交易 Merkle 根；
  6) 使用 RandomQ 哈希搜索 nonce，满足目标后调用 `submitblock`。

常见问题
- 提示 “Core is not connected” 或连接失败：
  - 确认节点已启用 RPC 且在本机 127.0.0.1 上监听；
  - 若修改了端口或地址，请同时在矿工中设置 `-rpcconnect`/`-rpcport`；
  - 使用 `-rpcwait`，在节点初始加载较慢时等待 RPC 就绪；
  - 若使用账户密码鉴权，确保与节点配置一致或改用 cookie。

- 提示地址无效：
  - 确认 `-address` 为目标链对应的合法地址（bech32）。

- 哈希率为 0：
  - 检查线程数与 CPU 亲和设置，尝试增大 `-threads` 或移除亲和限制；
  - 检查是否频繁刷新模板（`-maxtries` 太小）。

注意事项
- 请勿将 RPC 端口暴露到不可信网络。
- 在 signet/testnet4 等网络，可能需要为 `getblocktemplate` 启用相应规则（程序默认包含 `segwit`，可按需扩展）。


