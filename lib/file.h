


// 打开文件
XXAPI ptr xrtOpen(str sPath, size_t iSize, int bReadOnly, int iCharset)
{
	return NULL;
}



// 关闭文件
XXAPI void xrtClose(ptr objFile)
{
}



// 设置游标位置
XXAPI void xrtSeek(ptr objFile, int iOrigin, long iOffset)
{
}



// 获取游标位置
XXAPI size_t xrtTell(ptr objFile)
{
	return 0;
}



// 是否已经读取到文件末尾
XXAPI int xrtEOF(ptr objFile)
{
	return FALSE;
}



// 设置文件末尾
XXAPI int xrtSetEOF(ptr objFile)
{
	// ftruncate
	return FALSE;
}



// 从已打开的文件读取数据
XXAPI str xrtRead(ptr objFile, size_t iSize)
{
	return NULL;
}



// 向已打开的文件写入数据
XXAPI int xrtWrite(ptr objFile, str sText, size_t iSize)
{
	return FALSE;
}



// 从已打开的文件读取二进制数据
XXAPI ptr xrtGet(ptr objFile, size_t iSize)
{
	return NULL;
}



// 向已打开的文件写入二进制数据
XXAPI int xrtPut(ptr objFile, ptr pBuff, size_t iSize)
{
	return FALSE;
}



// 判断路径是否存在
XXAPI int xrtPathExists(str sPath)
{
	return FALSE;
}



// 判断文件是否存在
XXAPI int xrtFileExists(str sPath)
{
	return FALSE;
}



// 判断目录是否存在
XXAPI int xrtDirExists(str sPath)
{
	return FALSE;
}



// 获取文件长度
XXAPI size_t xrtFileGetSize(str sPath)
{
	return 0;
}



// 设置文件长度
XXAPI int xrtFileSetSize(str sPath, size_t iSize)
{
	return 0;
}



// 向文件追加写入数据
XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset)
{
	return 0;
}



// 写入并覆盖文件内容
XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset)
{
	return 0;
}



// 读取文件的全部内容
XXAPI int xrtFileReadAll(str sPath, int iCharset)
{
	return 0;
}



// 写入并覆盖文件内容（二进制）
XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize)
{
	return 0;
}



// 读取文件的全部内容（二进制）
XXAPI int xrtFileGetAll(str sPath)
{
	return 0;
}



// 获取文件属性
XXAPI int xrtFileGetAttr(str sPath)
{
	return 0;
}



// 设置文件属性
XXAPI int xrtFileSetAttr(str sPath, int Attr)
{
	return 0;
}



// 复制文件
XXAPI int xrtFileCopy(str sSrc, str sDst, int bReWrite)
{
	return 0;
}



// 移动文件（重命名）
XXAPI int xrtFileMove(str sSrc, str sDst, int bReWrite)
{
	return 0;
}



// 删除文件
XXAPI int xrtFileDelete(str sPath)
{
	return 0;
}



// 遍历文件夹
XXAPI int xrtDirScan(str sPath)
{
	return 0;
}



// 获取文件夹内容列表
XXAPI int xrtDirList(str sPath)
{
	return 0;
}



// 创建文件夹
XXAPI int xrtDirCreate(str sPath)
{
	return 0;
}



// 复制文件夹
XXAPI int xrtDirCopy(str sSrc, str sDst, int bReWrite)
{
	return 0;
}



// 移动文件夹
XXAPI int xrtDirMove(str sSrc, str sDst, int bReWrite)
{
	return 0;
}



// 删除文件夹
XXAPI int xrtDirDelete(str sPath)
{
	return 0;
}


