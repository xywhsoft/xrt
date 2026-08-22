# HTTP 正文组合

`http_body_compose` 在不把完整内容合并到连续缓冲区的前提下，把固定字节与已有 `xhttpbody` 顺序组合成一个正文。该模块只依赖 `http_body`，适合 multipart、批量协议封装和固定元数据加流式内容等上层构建器复用。

## 所有权

- `xrtHttpBodyPieceBytes` 描述借用输入，`xrtHttpBodyCompose` 返回前会复制这些字节。
- `xrtHttpBodyPieceBody` 描述借用正文，组合成功后由组合对象持有独立引用。
- 输入片段数组在 `xrtHttpBodyCompose` 返回后即可释放或修改。
- 返回的字节 `Chunk` 持有工厂引用，因此可以晚于 Reader 和正文对象释放。
- 片段数组和非空字节都必须形成不回绕的有效地址范围；失败不取得任何子正文所有权。
- `Kind` 未选择的字段必须为空，组合器不会猜测或修复有歧义的描述符。

## 长度与重放

- 所有片段长度已知时，组合正文公开精确长度；任一子正文长度未知时返回 `XHTTP_BODY_UNKNOWN`。
- 只有全部子正文均可重放时，组合正文才公开 `XHTTP_BODY_REPLAYABLE`。
- 子正文按需打开，任意时刻最多保留一个活动子 Reader。
- 启用 `http_body_async` 后，子正文的 `AGAIN` 与 `Wait` 会透明传递。
- Wait 返回的 Future 独立于组合 Reader；Reader 关闭后 Future 仍可安全完成和销毁。
- 子正文的 Open、Next 或 Wait 错误会成为组合 Reader 的稳定错误，不复用线程旧错误。

## 示例

```c
xhttpbodypiece Pieces[3];
xhttpbody* pValue = xrtHttpBodyBorrow(
	(xbytesview){ (cbytes)"42", 2 }
);
xhttpbody* pBody;

Pieces[0] = xrtHttpBodyPieceBytes(
	(xbytesview){ (cbytes)"{\"value\":", 9 }
);
Pieces[1] = xrtHttpBodyPieceBody(pValue);
Pieces[2] = xrtHttpBodyPieceBytes(
	(xbytesview){ (cbytes)"}", 1 }
);
pBody = xrtHttpBodyCompose(Pieces, 3);
xrtHttpBodyDestroy(pValue);
```

空片段数组返回可重放空正文；单个字节片段和单个正文片段会退化为已有固定正文或正文引用，不创建组合状态机。

