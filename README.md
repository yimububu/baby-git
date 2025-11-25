Baby-GIT - Initial revision of "git"
==========================================
为了学习 Git 的精髓，编译由 Linus Torvalds 于 2005 年 4 月 8 日创建的第一个提交版本（commit hash:
`e83c5163316f89bfbde7d9ab23ca2e25604af290`）。  
这个初始版本的 Git 代码库非常精简，仅包含不到 1000 行的 C 语言代码，通过对其进行编译和研究，可以深入理解 Git
的核心设计理念。  
注意：如果你正在寻找 `baby-git` 的 README 文件，它已经被重新命名为`README.jacobstopak`。

**当前源代码的提交信息**

```
Initial revision of "git", the information manager from hell
e83c5163 Linus Torvalds <torvalds@ppc970.osdl.org> on 2005/4/8 at 06:13
```

**环境**

- OS:macOS Sequoia 15.7.1
- IDE:CLion 2025.2.4
- brew:Homebrew 5.0

**命令**

```shell
$ git clone git@github.com:yimububu/baby-git.git 或者 git clone https://github.com/yimububu/baby-git.git
$ cd baby-git
$ make
fatal error: 'openssl/sha.h' file not found
```

**错误原因**

上述问题的根本原因在于 macOS 系统的特殊性：

1. **不再内置 OpenSSL**：出于安全和软件许可策略的考虑，苹果公司在其现代的 macOS 系统中已经不再提供 OpenSSL
   的头文件和开发库。转而推荐使用其自家的加密框架 (Common Crypto 或 CryptoKit)。
2. **Homebrew 路径**：在 macOS 上，我们通常使用包管理器 Homebrew 来安装像 OpenSSL 这样的第三方库。但是，Homebrew
   并不会将其安装的库链接到系统的标准路径（如 /usr/include 或 /usr/lib）下，以避免与系统自带的工具链产生冲突。

**解决方案**

调整 Makefile 文件，让编译系统使用 Homebrew 安装的 OpenSSL；

1. 首先，确保已经通过 `Homebrew` 安装了 `OpenSSL`。否则打开终端并运行：

```
brew install openssl
```

2. 修改 `Makefile` 文件；

```
找到下面这段代码：
ifeq ($(SYSTEM),Darwin)
    CFLAGS += -D BGIT_DARWIN
endif

替换为：
ifeq ($(SYSTEM),Darwin)
    # 为 macOS 配置 Homebrew 安装的 OpenSSL 路径
    # Configure paths for OpenSSL installed via Homebrew on macOS
    OPENSSL_PATH := $(shell brew --prefix openssl)
    CFLAGS += -D BGIT_DARWIN -I$(OPENSSL_PATH)/include
    LDFLAGS = -L$(OPENSSL_PATH)/lib
endif
```

```
在链接命令中使用 LDFLAGS
找到从 init-db 到 show-diff 的总共 7 个目标规则，它们的格式都类似于：
init-db      : init-db.o $(RCOBJ)
	$(CC) $(CFLAGS) -o $@ $@.o $(RCOBJ) $(LDLIBS)
	
在 $(CFLAGS) 和 -o 之间加入 $(LDFLAGS)
init-db      : init-db.o $(RCOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $@.o $(RCOBJ) $(LDLIBS)
```

然后执行 `make` 或 `make clean`。

**当前方案**

因为上述配置，`make` 后根目录存在很多`*.o`,`可执行文件`，调整 `Makefile`，将 `*.o` 输出到 `obj`，将 `可执行文件` 输出到
`target`。

**Debug步骤**  

1. 创建运行配置 (Run/Debug Configuration)：
   - 点击 CLion 右上角的运行配置下拉框（"Edit Configurations..."）。
   - 选择 Edit Configurations...。
   - 点击左上角的 + 号，选择 Custom Build Application (自定义构建应用程序)。
2. 配置调试参数：
   - Name: 给个名字，例如 Debug init-db。
   - Target: 选择 Configure Custom Build Targets。**一个空的 Target，目的是骗过 CLion 让它以为构建完成了**
   - Executable (重要): 浏览并选择你编译出来的那个二进制文件（例如 init-db 或 update-cache）。
   - Program arguments: 如果该命令需要参数（比如 update-cache filename.c），在这里填入。对于 init-db 通常不需要参数。
   - Working directory (非常重要): 确保这里指向的是你的源码根目录。因为 Git 的这些命令会在当前目录下创建 .dircache 目录（Git 早期不叫 .git），路径不对会报错。
3. 开始调试：
   - 在源码中（例如 init-db.c 的 main 函数里）点击行号旁边，打上断点。
   - 点击右上角的 绿色虫子图标 (Debug)。
   - 程序应该会启动并停在你的断点处。

**注意**：  
1. Makefile 里的 -O3 参数会导致无法断点调试。-O3 是最高级别的优化，编译器会把代码顺序打乱、内联函数，导致源码和机器码对不上号。
```shell
CFLAGS  = -g -Wall -O3
# CFLAGS  = -g -Wall # 去掉 -O3 即可
```
2. 重新 make clean & make。因为 obj 和 target 中编译出的旧的带 “-O3” 优化的编译结果，现在 Debug 需要不带 “-O3” 的编译结果。