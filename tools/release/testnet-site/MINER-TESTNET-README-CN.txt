RIGOR TESTNET SOLO 挖矿说明  v0.2

Network: TESTNET ONLY
这不是矿池。没有 stratum+tcp:// 地址。
矿工自己填写 RPC 用户名、密码和 trg1q 收款地址。

不要用主网钱包、rg1q、8760、8761。

================================================
第 1 步  打开测试网钱包，找到 data 文件夹
================================================
1. 打开 Rigor Wallet Testnet（点 rigor-wallet.exe）。
2. 窗口标题必须带 [testnet]。
3. rigor.conf 只能放在钱包的 data 文件夹里：
   和 rigor-wallet.exe 同一层 → 打开 data → 再保存 rigor.conf
   不要和 rigor-wallet.exe 放在同一层，那样钱包完全读不到。
   不要保存到 testnet3 里，也不要按以前安装留下的盘符。
4. 如果还没有 data 文件夹，先打开一次钱包，它会自动建立。

================================================
第 2 步  用记事本新建 rigor.conf
================================================
1. 打开 Windows 记事本（Notepad）。
2. 不要打开已有的 .txt 文件。点：文件 → 新建。
3. 把下面 5 行原样粘贴进去（YOUR_NAME 和 YOUR_PASSWORD
   改成你自己定的用户名和密码，不要用 CHANGE_ME）：

testnet=1
server=1
rpcuser=YOUR_NAME
rpcpassword=YOUR_PASSWORD
rpcallowip=127.0.0.1

4. 点：文件 → 保存。
5. 浏览到钱包安装目录下的 data 文件夹。
   正确：钱包安装目录\data\rigor.conf
   错误：钱包安装目录\rigor.conf
   （和 rigor-wallet.exe 放一起时，钱包读不到，矿工会认证失败。）
6. 文件名必须写成：

   rigor.conf

7. 保存类型必须选：

   所有文件

   切记：不要选「文本文档」。
   选错会变成 rigor.conf.txt，钱包读不到，挖矿会失败。

8. 保存后在该文件夹里确认文件名是 rigor.conf，
   不是 rigor.conf.txt。如果是 .txt，删掉，按上面重做。

================================================
第 3 步  彻底关闭钱包
================================================
这一步必须单独做完，不要跳过。

1. 关掉 Rigor Wallet Testnet 主窗口。
2. 看屏幕右下角托盘。如果还有 Rigor 图标，
   右键图标 → 退出 / Exit / Quit。
3. 打开任务管理器，确认没有 rigor-qt.exe、
   rigor-wallet.exe、rigord.exe。
4. 确认钱包已经完全退出后，才能做第 4 步。
   不关干净，新的 rigor.conf 不会生效。

================================================
第 4 步  重新打开钱包
================================================
1. 再打开 Rigor Wallet Testnet。
2. 再确认标题带 [testnet]，收款地址是 trg1q 开头。
3. 等到钱包可以正常使用。

================================================
第 5 步  复制 trg1q 收款地址
================================================
钱包 Receive（收款）里复制一个 trg1q 开头的地址。
不要用 rg1q。

================================================
第 6 步  用记事本编辑矿工脚本
================================================
1. 用记事本编辑 rigor-miner-testnet.cmd
   不要双击它来改内容。双击是运行矿工。
   正确做法：右键该文件 → 编辑；
   或先打开记事本，再用「文件 → 打开」选这个文件。
2. 先改这三行（必须和第 2 步 rigor.conf 完全相同）：

   RPC_USER
   RPC_PASS
   COINBASE_ADDR

   COINBASE_ADDR 填第 5 步复制的 trg1q 地址。
3. 再加线程限制，不要占满 CPU。
   本机还要跑钱包和节点。线程数 = CPU 核数减 1 或减 2，最少为 1。
   例如 8 核用 -t 6，4 核用 -t 2。
   任务管理器 → 性能 → CPU 可看「逻辑处理器」个数。
   写法二选一：
   - 脚本里有 MINER_THREADS 或 -t：只改那个数字。
   - 没有这两项：在 rigor-miner.exe 那一行末尾加上 -t N
     （N 换成上面算出的数字）。
   不要省略 -t。不写 -t 时，矿工会默认用满全部 CPU。
4. 保存。
5. 保存后再双击 rigor-miner-testnet.cmd 运行。
6. 黑窗口要一直开着。出现算力行才是在挖。

不要双击 rigor-miner.exe。

================================================
Linux
================================================
在钱包程序旁的 data 目录新建 rigor.conf，内容与第 2 步相同。
文件名必须是 rigor.conf，不要 rigor.conf.txt。

彻底退出钱包进程后，再启动钱包，然后用文本编辑器编辑 miner-testnet.sh
或直接运行（把 N 换成「核数减 1 或减 2」，最少 1）：

  ./bin/rigor-miner -a rgrpow01 -o http://127.0.0.1:18761 \
    -u YOUR_NAME -p YOUR_PASSWORD --coinbase-addr=trg1qYOURADDRESS \
    -t N
