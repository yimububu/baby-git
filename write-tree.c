/*
 *  All documentation (comments) unless explicitly noted:
 *
 *      Copyright 2018, AnalytixBar LLC, Jacob Stopak
 *
 *  All code & explicitly labelled documentation (comments):
 *      
 *      Copyright 2005, Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 **************************************************************************
 *
 *  The purpose of this file is to be compiled into an executable
 *  called `write-tree`. When `write-tree` is run from the command line
 *  it does not take any command line arguments.
 *
 *  The `write-tree` command takes the changes that have been staged in
 *  the index and creates a tree object in the object store recording
 *  these changes.
 *
 *  Everything in the main function in this file will run
 *  when ./write-tree executable is run from the command line.
 */

#include "cache.h"
/* The above 'include' allows use of the following functions and
   variables from "cache.h" header file, ranked in order of first use
   in this file. Most are functions/macros from standard C libraries
   that are `#included` in "cache.h". Function names are followed by
   parenthesis whereas variable/struct names are not:

   -sha1_file_name(): Build the path of an object in the object database
                      using the object's SHA1 hash value.

   -access(path, amode): Check whether the file at `path` can be accessed
                         according to the permissions specified in `amode`.

   -perror(message): Write `message` to standard error output stream. Sourced
                     from <stdio.h>.

   -read_cache(): Read the contents of the `.dircache/index` file into the
                  `active_cache` array. The number of caches entries is 
                  returned.

   -fprintf(stream, message, ...): Write `message` to the output `stream`. 
                                   Sourced from <stdio.h>.

   -exit(status): Stop execution of the program and exit with code `status`.
                  Sourced from <stdlib.h>.

   -malloc(size): Allocate unused space for an object whose size in bytes is 
                  specified by `size` and whose value is unspecified. Sourced 
                  from <stdlib.h>.

   -alloc_nr(x): This is a macro in "cache.h" that's used to calculate the
                 maximum number of elements to allocate to the active_cache 
                 array.

   -realloc(pointer, size): Update the size of the memory object pointed to by
                            `pointer` to `size`. Sourced from <stdlib.h>.

   -sprintf(s, message, ...): Writes `message` string constant to string 
                              variable `s` followed by the null character 
                              '\0'. Sourced from <stdio.h>.

   -memcpy(s1, s2, n): Copy n bytes from the object pointed to by s2 into the 
                       object pointed to by s1.

   -write_sha1_file(): Deflate an object, calculate the hash value, then call
                       the write_sha1_buffer function to write the deflated
                       object to the object database.

   ****************************************************************

   The following variables and functions are defined in this source file.

   -main(): The main function runs each time the ./write-tree command is run.

   -check_valid_sha1(): Check if user-supplied SHA1 hash corresponds to an
                        object in the object database.

   -prpend_integer(): Prepend a string containing the decimal form of the size 
                      of the tree data in bytes to the buffer.

   -ORIG_OFFSET: Token that defines the number of bytes at the beginning of 
                 the buffer that are allocated for the object tag and the 
                 object data size.
*/

/*
 * Function: `check_valid_sha1`
 * Parameters:
 *      -sha1: An SHA1 hash to check.
 * Purpose: Check if user-supplied SHA1 hash corresponds to an object in the 
 *          object database and if the process has read access to it.
 */
static int check_valid_sha1(unsigned char *sha1) // 验证某 SHA1 对象文件是否可读。
{
    /*
     * Build the path of an object in the object database using the object's 
     * SHA1 hash value.
     */
    char *filename = sha1_file_name(sha1); // 把 20 字节 SHA1 转对象路径（例如 .dircache/objects/ab/cdef...）
    int ret;   /* Return code. 保存 access 返回码*/

    /*
     * Check whether the process has read access to the object in the object 
     * database. 
     */
    ret = access(filename, R_OK); // 检查对象文件是否存在且可读

    /* Error if the file is not accessible. 若不可读/不存在进入错误分支*/
    if (ret)
        perror(filename);

    return ret; // 0 表示有效，非 0 表示无效
}

/*
 * Function: `prepend_integer`
 * Parameters:
 *      -buffer: Pointer to buffer that holds the tree data.
 *      -val: Size in bytes of the tree data.
 *      -i: Number of bytes at the beginning of `buffer` that are allocated 
 *          for the object tag and object data size.
 * Purpose: Prepend a string containing the decimal form of the size of the 
 *          tree data in bytes to the buffer.
 */
static int prepend_integer(char *buffer, unsigned val, int i) // 把十进制数字串逆向写到 buffer 前部，返回新起始下标
{
    /* Prepend a null character to the tree data in the buffer. */
    buffer[--i] = '\0'; // 先放对象头结束符 \0，为 "tree <size>\0" 预留尾部

    /*
     * Prepend a string containing the decimal form of the size of the tree 
     * data in bytes before the null character.
     */
    do { // 开始逐位写入十进制数字（从低位到高位逆向写）
        buffer[--i] = '0' + (val % 10); // 写当前最低位数字字符
        val /= 10; // 丢弃最低位
    } while (val); // 直到所有位写完
    /*
     * The value of `i` is now the index of the buffer element that contains 
     * the most significant digit of the decimal form of the tree data size.
     */
    return i; // 返回数字串首字符下标
}

/* Linus Torvalds: Enough space to add the header of "tree <size>\0" */
#define ORIG_OFFSET (40) // 预留 40 字节头部空间，后面先写 tree 数据体，再回填 "tree <size>\0"

/*
 * Function: `main`
 * Parameters:
 *      -argc: The number of command-line arguments supplied, inluding the 
 *             command itself. 
 *      -argv: An array of the command line arguments, including the command 
 *             itself.
 * Purpose: Standard `main` function definition. Runs when the executable 
 *          `write-tree` is run from the command line. 
 */
int main(int argc, char **argv) // 程序入口
{
    /* The size to be allocated to the buffer. 动态缓冲区容量*/
    unsigned long size;
    /* Index of the buffer element to be filled next. 当前写入偏移位置*/
    unsigned long offset;
    /* Not used. Even Linus Torvalds makes mistakes. 未使用变量*/
    unsigned long val;
    /* Iterator used in for loop. 循环变量/临时下标*/
    int i;
    /*
     * Read in the contents of the `.dircache/index` file into the 
     * `active_cache` array. The number of cache entries is returned and 
     * stored in `entries`.
     */
    int entries = read_cache(); // 把 .dircache/index 读入 active_cache，返回条目数

    /* String to hold the tree's content. */
    char *buffer; // tree 对象构造缓冲区

    /*
     * If there are no active cache entries or if there was an error reading
     * the cache, display an error message and exit since there is nothing to 
     * write to a tree.
     */
    if (entries <= 0) { // 无条目或读索引失败
        fprintf(stderr, "No file-cache to create a tree of\n"); // 没有可构建树的数据
        exit(1);
    }

    /* Linus Torvalds: Guess at an initial size 猜测初始容量，减少频繁 realloc*/
    size = entries * 40 + 400;
    /* Allocate `size` bytes to buffer to store the tree content. 分配缓冲区*/
    buffer = malloc(size);
    /*
     * Set the offset index using the macro defined in this file. The tree
     * data will be written starting at this offset.  The tree metadata will
     * be written before it.
     */
    offset = ORIG_OFFSET; // 从 40 偏移处开始写 tree 数据体

    /*
     * Loop over each cache entry and build the tree object by adding the 
     * data from the cache entry to the buffer.
     */
    for (i = 0; i < entries; i++) { // 遍历所有 index 条目
        /* Pick out the ith cache entry from the active_cache array. */
        struct cache_entry *ce = active_cache[i]; // 取第 i 个缓存条目

        /* Check if the cache entry's SHA1 hash is valid. Otherwise, exit. */
        if (check_valid_sha1(ce->sha1) < 0) // 确保该条目引用的 blob 对象文件确实存在且可读
            exit(1); // 对象缺失则直接失败退出

        /* If needed, increase the size of the buffer. */
        if (offset + ce->namelen + 60 > size) { // 空间不足时扩容；60 为保守余量（mode、空格、\0、sha1等）
            size = alloc_nr(offset + ce->namelen + 60); // 按增长策略算新容量
            buffer = realloc(buffer, size); // 实际扩容。
        }

        /*
         * Write the cache entry's file mode and name to the buffer and
         * increment `offset` by the number of characters that were written.
         */
        offset += sprintf(buffer + offset, "%o %s", ce->st_mode, ce->name); // 写入 tree entry 的文本前半："<mode> <name>"，并推进偏移

        /*
         * Write a null character to the buffer as a separator and increment
         * `offset`. 
         */
        buffer[offset++] = 0; // 追加 NUL 分隔符，tree 格式要求

        /* Add the cache entry's SHA1 hash to the buffer. */
        memcpy(buffer + offset, ce->sha1, 20); // 追加 20 字节二进制 SHA1

        /*
         * Increment the offset by 20 bytes, the length of an SHA1 hash.
         */
        offset += 20; // 偏移前进 20 字节
    }

    /*
     * Prepend a string containing the decimal form of the size of the tree 
     * data in bytes to the buffer.
     */
    i = prepend_integer(buffer, offset - ORIG_OFFSET, ORIG_OFFSET); // 把 tree 数据体大小（offset-40）十进制写回头部区域，得到大小串起点
    /*
     * Prepend the string `tree ` to the buffer to identify this object as a 
     * tree in the object store.
     */
    i -= 5; // 再向前挪 5 字节给 "tree "
    memcpy(buffer+i, "tree ", 5); // 写入对象类型头 "tree "。

    /*
     * Adjust buffer to start at the first character of the `tree` object 
     * tag. 
     */
    buffer += i; // 把指针移动到对象真正起始位置（'t'）
    /* Calculate final total size of this buffer. */
    offset -= i; // 计算最终对象总长度（头 + 数据体）

    /* Compress the contents of buffer, calculate SHA1 hash of compressed 
     * output, and write the tree object to the object store. 
     */
    write_sha1_file(buffer, offset); // 压缩对象、计算 SHA1、写入对象库，并在该实现里打印 SHA1。

    /* Return success. */
    return 0;
}
