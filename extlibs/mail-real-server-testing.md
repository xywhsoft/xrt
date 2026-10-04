# 独立邮件服务器验收

`tools/test_mail_real_servers.py` 运行实际 Dovecot 2.4 与 Postfix 实例，调用
现有 SMTP、POP3、IMAP C 范例。所有监听均在 IPv4 回环地址，邮件只投递到
临时 `example.test` 邮箱。此结果是独立服务器实现的互操作证据；真实运营商
账户、网络策略和部署环境仍需单独验收。

在 Ubuntu 26.04 x86_64 环境从仓库根目录执行以下命令。需要 Python 3.11
以上、GCC、OpenSSL、APT 工具、`ps`、`mount` 与 `unshare`。服务器依赖库
必须在宿主或解包运行时中可用；版本探针或服务启动失败会终止测试。
准备工具只下载和解包软件包，记录仓库 SHA256、准确版本及解包文件 SHA256，
不安装服务器。输出目录必须尚不存在。

```sh
python3 tools/prepare_mail_servers.py --output /var/tmp/xrt-mail-runtime
sudo unshare --mount --propagation private -- python3 tools/test_mail_real_servers.py \
  --runtime /var/tmp/xrt-mail-runtime --output /var/tmp/xrt-mail-test
```

运行器要求 root 和独立 mount namespace，为服务器编译时固定的模块目录
建立私有挂载。源码可位于普通用户的仓库中；服务状态目录应使用短的本地
ASCII 路径（最长 70 字节），避免 Unix socket 路径长度和配置语法限制。
运行器自行生成测试密码和 CA，仅将密码交给子进程环境或 Windows 桥接的
标准输入，结果文件不包含密码。正常及异常退出清理密码文件和私钥，并停止
本次启动的服务，检查所有监听端口已关闭。

十八个场景覆盖三种协议、隐式 TLS 与 STARTTLS/STLS，以及正常访问、错误
密码和错误证书身份。SMTP 检查实际 LMTP 投递后的发件人、收件人、主题和
含点行的正文；POP3 检查实际取回的邮件；IMAP 检查实际邮箱计数。负向场景
检查客户端的明确错误类别/代码，以及服务端的认证失败或握手失败记录。
结果保存源码与构建输入、客户端二进制、运行时清单的哈希，并核对运行前后
未变。没有添加覆盖率编译选项或执行旧的计量二进制。

Windows 客户端可通过 WSL localhost forwarding 连接同样的服务。以下命令
在 WSL 仓库目录运行，按机器配置填写 Windows Python、桥接脚本和编译器路径。
桥接脚本必须来自同一份源码；Windows 路径使用斜杠，避免 WSL 参数转义。

```sh
sudo unshare --mount --propagation private -- python3 tools/test_mail_real_servers.py \
  --runtime /var/tmp/xrt-mail-runtime --output /var/tmp/xrt-mail-win-test \
  --windows-python '/mnt/c/Program Files/Python312/python.exe' \
  --windows-bridge 'D:/GIT/xrt/tools/mail_server_windows_bridge.py' \
  --compiler 'E:/software/w64devkit/bin/gcc.exe' --wsl-distribution Ubuntu
```

每次验收使用新的输出目录，读取其中的 `results.json`、`syslog.log` 和
`dovecot-info.log`。以 `all_passed` 为最终判定，原始失败目录应保留。
GitHub Actions 的 `mail-real-servers.yml` 使用固定摘要的 Ubuntu 26.04 容器
运行 Linux 场景并保存报告；工作流配置本身不构成托管运行通过的证据。

服务器配置参考 [Dovecot 2.4 文档](https://doc.dovecot.org/2.4.2/core/config/quick.html)
和 [Postfix 实例文档](https://www.postfix.org/MULTI_INSTANCE_README.html)。
Ubuntu 镜像摘要来自 [Docker 官方镜像元数据](https://github.com/docker-library/repo-info/blob/master/repos/ubuntu/remote/26.04.md)。
