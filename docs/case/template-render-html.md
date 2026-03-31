# 用 Template 渲染一个 HTML 页面

> 这个案例的重点不是 HTML 语法，而是说明：在 XRT 当前主线里，`template + xvalue` 怎样形成一条很自然的页面渲染链。

[返回案例索引](README.md)

---

## 1. 场景

假设你需要生成一个简单 HTML 页面，用来展示：

- 标题
- 用户名称
- 状态文本
- 一组标签

如果只是拼字符串，短期能跑；但一旦页面结构和数据都开始增长，后面会越来越难维护。

在 XRT 当前主线里，更推荐：

- 数据模型：`xvalue`
- 输出模板：`template`


## 2. 为什么推荐 `template + xvalue`

这两者组合的好处非常直接：

- 页面结构和业务数据分离
- 模板可读性更高
- 数据可以直接复用到 JSON、配置、HTTP 输出
- 后续扩展字段时，不需要重写整段字符串拼接逻辑

也就是说，它更像现代模板引擎的工作方式，而不是传统 C 程序里满屏 `sprintf`。


## 3. 推荐主线

这条链推荐这样理解：

1. 先用 `xvalue` 组织页面数据
2. 再把 `xvalue` 作为模板上下文
3. 最后把结果输出成 HTML

一个最小上下文大致可以这样想：

```c
xvalue vPage = xvoCreateTable();
xvalue vTags;

xvoTableSetText( vPage, "title", 0, "XRT Demo", 0, FALSE );
xvoTableSetText( vPage, "name", 0, "Alice", 0, FALSE );
xvoTableSetText( vPage, "status", 0, "running", 0, FALSE );

xvoTableSetArray( vPage, "tags", 0 );
vTags = xvoTableGetValue( vPage, "tags", 0 );
xvoArrayAppendText( vTags, "runtime", 0, FALSE );
xvoArrayAppendText( vTags, "network", 0, FALSE );
xvoArrayAppendText( vTags, "template", 0, FALSE );
```

这时，页面上下文本质上已经是一棵结构化数据树。


## 4. 为什么不用手工拼 HTML 字符串

手工拼 HTML 在最小示例里看起来简单，但很快会遇到这些问题：

- 结构难读
- 修改字段时容易漏
- 列表渲染和条件渲染变乱
- 页面和数据逻辑耦合太深

如果站在 XRT 当前主线的思路看，模板不是“锦上添花”，而是让数据层和展示层保持清晰边界的关键工具。


## 5. xvalue 在这里为什么特别合适

因为页面通常本来就是一个嵌套数据模型：

- 页面元信息
- 用户对象
- 标签数组
- 条件状态
- 可选区块

`xvalue` 刚好很适合承载这种：

- table
- array
- string / int / bool

的混合结构。

而且它还能和 JSON、配置、网络输出共用同一套数据主线。


## 6. 这条链如何继续扩展

一个最小渲染页，后续通常会自然扩展成：

- 更复杂的 layout
- 列表区块
- 表格
- 条件显示
- 国际化文本
- 把 HTTP 服务直接接到模板输出

如果你的起点就是 `template + xvalue`，这些扩展通常都还能沿着同一条主线前进。


## 7. 和 HTTP 服务怎么接

这条链和前面的 HTTP 服务案例天然是兼容的：

- HTTP 服务负责接请求
- `xvalue` 负责组织页面上下文
- template 负责渲染 HTML

这样 `JSON API` 和 `HTML 页面` 也可以共享一套数据准备逻辑，而不是分裂成两套完全不同的实现。


## 8. 常见错误

### 错误一：模板只是做文本替换，数据还是到处散

如果数据模型没有统一收进 `xvalue`，模板层的收益会被削弱很多。

### 错误二：把展示逻辑和数据构造混在一起

推荐先构造完整上下文，再渲染。

### 错误三：为一个页面单独发明另一套数据结构

如果程序内部已经有 `xvalue` 主线，页面层通常就不应该再平行维护第二套对象模型。


## 9. 建议继续阅读

- [Template API](../api/api-template.md)
- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [用 xvalue + json 构造配置系统](json-config-system.md)
- [用 XRT 写一个最小 HTTP 服务](minimal-http-service.md)

---

**一句话总结：** 在当前 XRT 主线里，`template + xvalue` 是组织 HTML 输出最自然的一条链：模板负责结构，`xvalue` 负责数据，两者组合起来比手工拼字符串更稳定、更容易扩展。
