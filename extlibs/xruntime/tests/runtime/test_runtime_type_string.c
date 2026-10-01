#include "../test.h"



/* 验证拥有型字符串描述、事务复制、移动、内容比较和散列。 */
int main(void)
{
	{
		const xrttype* pViewType = xrtTypeStringView();
		xstrview Source = {"a\0b", 3}, Target = {0}, Moved = {0}, Compare = {"a\0c", 3};
		testRequire(xrtTypeValidate(pViewType) && pViewType->Size == sizeof(xstrview), "view descriptor");
		testRequire(xrtTypeCopyValue(pViewType, &Target, &Source), "view copy");
		testRequire(Target.Size == 3 && Target.Data != Source.Data && !memcmp(Target.Data, Source.Data, 3), "view exact bytes");
		testRequire(pViewType->Ops->Compare(&Target, &Compare, pViewType) < 0, "view NUL ordering");
		testRequire(pViewType->Ops->Hash(&Target, pViewType) == xrtHash64(Source.Data, Source.Size), "view hash");
		testRequire(xrtTypeMoveValue(pViewType, &Moved, &Target) && !Target.Data && !Target.Size, "view detach");
		xrtTypeDropValue(pViewType, &Moved);
		testRequire(!Moved.Data && !Moved.Size, "view drop");
	}
	const xrttype* pType = xrtTypeString();
	str sSource = xrtStrDup("alpha");
	str sTarget = xrtStrDup("old");
	str sClone;
	str sMoved;
	str sNull;
	str sEmpty = xrtStrDup("");
	uint64 iSourceHash;
	uint64 iCloneHash;
	uint64 iNullHash;
	uint64 iEmptyHash;
	int iCompare;

	testRequire(
		(sSource != NULL) && (sTarget != NULL) && (sEmpty != NULL),
		"runtime string type fixture failed"
	);
	testRequire(
		xrtTypeValidate(pType) &&
		(pType->Id == xrtTypeId(XRT_STR_LITERAL("xrt.string"))) &&
		(pType->Kind == XRT_TYPE_STRING) &&
		(pType->Size == sizeof(str)) &&
		xrtTypeIsCopyable(pType) &&
		xrtTypeIsRelocatable(pType) &&
		xrtTypeIsComparable(pType) &&
		xrtTypeIsHashable(pType),
		"runtime string type descriptor is invalid"
	);
	testRequire(
		xrtTypeInitValue(pType, &sClone) &&
		xrtTypeInitValue(pType, &sMoved) &&
		xrtTypeInitValue(pType, &sNull) &&
		(sClone == NULL) && (sMoved == NULL) && (sNull == NULL),
		"runtime string type init mismatch"
	);
	testRequire(
		xrtTypeCopyValue(pType, &sTarget, &sSource) &&
		(sTarget != sSource) &&
		(strcmp(sTarget, sSource) == 0) &&
		xrtTypeCopyValue(pType, &sTarget, &sTarget),
		"runtime string type copy mismatch"
	);
	testRequire(
		xrtTypeCloneValue(pType, &sClone, &sSource) &&
		(sClone != sSource) &&
		(strcmp(sClone, sSource) == 0),
		"runtime string type clone mismatch"
	);
	testRequire(
		xrtTypeCompareValue(pType, &sSource, &sClone, &iCompare) &&
		(iCompare == 0) &&
		xrtTypeHashValue(pType, &sSource, &iSourceHash) &&
		xrtTypeHashValue(pType, &sClone, &iCloneHash) &&
		(iSourceHash == iCloneHash),
		"runtime string type content identity mismatch"
	);
	testRequire(
		xrtTypeCompareValue(pType, &sNull, &sEmpty, &iCompare) &&
		(iCompare == 0) &&
		xrtTypeHashValue(pType, &sNull, &iNullHash) &&
		xrtTypeHashValue(pType, &sEmpty, &iEmptyHash) &&
		(iNullHash == iEmptyHash),
		"runtime string null and empty semantics mismatch"
	);
	testRequire(
		xrtTypeMoveValue(pType, &sMoved, &sClone) &&
		(sClone == NULL) && (sMoved != NULL) &&
		(strcmp(sMoved, "alpha") == 0) &&
		xrtTypeMoveValue(pType, &sMoved, &sMoved),
		"runtime string type move mismatch"
	);

	xrtTypeDropValue(pType, &sSource);
	xrtTypeDropValue(pType, &sTarget);
	xrtTypeDropValue(pType, &sClone);
	xrtTypeDropValue(pType, &sMoved);
	xrtTypeDropValue(pType, &sNull);
	xrtTypeDropValue(pType, &sEmpty);
	testRequire(
		(sSource == NULL) && (sTarget == NULL) &&
		(sClone == NULL) && (sMoved == NULL) &&
		(sNull == NULL) && (sEmpty == NULL),
		"runtime string type drop mismatch"
	);
	xrtClearError();
	printf("[PASS] runtime string type\n");
	return 0;
}
