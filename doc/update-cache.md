## update-cache.c 作用与代码讲解

`update-cache.c` 编译后的命令是 `update-cache`，在 Git 初始版本中承担“把工作区文件加入索引并写入对象库”的职责。  
从能力上看，它就是今天 `git add` 的早期前身。

这个文件做的核心事情有三步：

1. 把文件内容包装成 `blob <size>\0 + 文件内容`，压缩并计算 SHA-1，写入对象库。  
2. 更新内存中的 `active_cache`（索引条目数组，按路径字典序维护）。  
3. 把新索引写到 `.dircache/index.lock`，再原子 `rename` 成 `.dircache/index`。

---

## 为什么需要“索引”（设计理念）

先给一个精确定义：`index`（也常称 `staging area`）是“下一次提交的候选快照”，不是工作区的镜像，也不是历史提交本身。  
`git commit` 默认提交的是 index 的内容，而不是你此刻工作区里所有未提交内容。

索引存在的意义，主要有四点：

1. 解耦“编辑”和“提交”  
   开发时你可以持续修改工作区，但只把其中一部分放进 index。这样一次提交可以只表达一个清晰意图，而不是把当天所有改动打包在一起。

2. 支持增量构造提交  
   `git add` 可以多次执行，按文件、按路径，甚至按 hunk（`-p`）逐步把变更放入 index。  
   这让“逻辑上干净的提交历史”成为可操作流程，而不是事后整理。

3. 作为提交前的稳定边界  
   工作区可继续变化，但 index 在一次提交前是可审查、可比较、可回退的（例如 `git diff --cached`、`git restore --staged`）。  
   这种“先暂存再提交”的边界，降低了误提交流水风险。

4. 作为 Git 内部操作的中间层  
   从实现角度看，index 也是 Git 在多种操作间共享的中间表示：`add/reset/checkout/merge` 都会与它交互。  
   它并不只服务“提交前暂存”这一个场景，也承载了内部状态协调职责。

### 术语补充：为什么既有 index 又有 staging area
 
- 对多数用户来说，`staging area` 比 `index` 更直观。  
- 但在 Git 内部与 plumbing 语义里，`index` 这个术语仍然有技术上的历史连续性。  

因此一个更务实的理解是：  
- 面向使用者讲流程时，用“暂存区（staging area）”更贴近心智模型；  
- 面向实现细节和源码时，用“索引（index）”更精确、更可对齐底层结构。

---

## 按代码行说明（不含注释行）

### 1) 头文件与平台宏

- `L38` 引入 `cache.h`，获得索引结构、对象写入函数、全局缓存数组、跨平台宏等定义。  
- `L240-L248` 定义 `RENAME` 与 `RENAME_FAIL`，在 Unix/Windows 上统一“锁文件替换正式索引文件”的语义。

### 2) 路径比较与定位

- `cache_name_compare` (`L259-L273`)：按字典序比较两个路径名。  
  先比较公共前缀，前缀相同再按长度判定，保证全序关系。

- `cache_name_pos` (`L283-L307`)：在 `active_cache` 中二分查找位置。  
  返回值约定：  
  - 找到完全匹配：返回 `-pos-1`（负值编码“已存在”）  
  - 未找到：返回应插入的位置 `pos`（非负）

### 3) 删除与插入索引条目

- `remove_file_from_cache` (`L315-L325`)：如果路径存在，删除条目并 `memmove` 左移后续项。  
- `add_cache_entry` (`L334-L366`)：  
  - 已存在则直接替换。  
  - 不存在则必要时扩容（`alloc_nr` + `realloc`），再 `memmove` 右移并插入。  
  - 始终维持 `active_cache` 按路径有序，便于二分查找。

### 4) 文件对象化：`index_fd`

- `index_fd` (`L380-L479`) 把一个打开的文件转成对象并写入对象库：  
  1. 分配输出缓冲与元数据缓冲。  
  2. `mmap` 文件内容（Windows 走映射 API）。  
  3. 构造对象头：`blob <size>\0`。  
  4. 用 zlib 压缩“对象头 + 文件内容”。  
  5. 对压缩结果做 SHA-1，写入 `ce->sha1`。  
  6. 调 `write_sha1_buffer` 按哈希落盘到对象库。

这体现了 Git 最核心模型：内容寻址对象存储（内容决定 ID）。

### 5) 单文件加入索引：`add_file_to_cache`

- `add_file_to_cache` (`L491-L567`)：  
  1. `open` 文件，失败且 `ENOENT` 时从索引删除该路径（文件被删时同步索引）。  
  2. `fstat` 读取元数据。  
  3. 分配 `cache_entry`，填充 `ctime/mtime/dev/ino/mode/uid/gid/size/namelen/name`。  
  4. 调 `index_fd` 写对象并生成 `sha1`。  
  5. 调 `add_cache_entry` 更新有序索引数组。

### 6) 索引写盘：`write_cache`

- `write_cache` (`L580-L621`)：  
  1. 构造 `cache_header`（`signature/version/entries`）。  
  2. 对“头部（不含 sha1 字段）+ 所有 entry”计算 SHA-1。  
  3. 把哈希写入 `hdr.sha1`。  
  4. 将 header 和 entries 依次写到 `index.lock`。

这让索引文件具有完整性校验能力。

### 7) 路径合法性：`verify_path`

- `verify_path` (`L631-L648`) 会拒绝危险或含糊路径：  
  - 包含 `//`  
  - 段首为 `.`（包括 `.`、`..`、`.xxx`）  
  - 以 `/` 结尾  

这样避免把隐藏路径和歧义路径加入索引，减少安全与一致性问题。

### 8) 主流程：`main`

- `main` (`L660-L756`)：  
  1. 读取现有索引（`read_cache`）。  
  2. 创建独占锁文件 `.dircache/index.lock`（`O_EXCL` 防并发覆盖）。  
  3. 遍历命令行路径，先 `verify_path`，再 `add_file_to_cache`。  
  4. 全部成功后 `write_cache` 写锁文件。  
  5. 通过 `rename(index.lock, index)` 原子替换正式索引。  
  6. 失败路径统一清理并删除锁文件。

---

## 在 Git 初始提交中的作用

`update-cache.c` 是初始 Git 的关键枢纽：它把“工作区文件”连接到“对象库 + 索引”。  
没有它，`write-tree` 就拿不到完整索引，`commit-tree` 也无法基于树对象形成可追踪提交。

也就是说，早期提交流程最小闭环是：

1. `init-db`：初始化对象库目录结构  
2. `update-cache`：把文件写成对象并登记到索引  
3. `write-tree`：从索引生成 tree  
4. `commit-tree`：生成 commit

其中 `update-cache` 负责第二步，是“暂存语义”真正落地的地方。

---

## 参考资料

- https://felipec.wordpress.com/2021/08/10/git-staging-area-rename/  
- https://www.reddit.com/r/programming/comments/p26bp4/the_git_staging_area_the_term_literally_everyone/?tl=zh-hans  
- https://git-scm.com/docs/git-add  
- https://git-scm.com/docs/git-commit  
- https://git-scm.com/docs/gitdatamodel
