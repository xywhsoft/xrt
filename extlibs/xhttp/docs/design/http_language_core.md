# HTTP Language Core

该内部模块只实现 RFC 4647 basic language range 所需的无分配标签校验，供
RFC 8187 扩展值和 Accept-Language 共用。它不公开独立 API，也不引入语言协商
对象；选择上层模块时由裁剪依赖自动带入。
