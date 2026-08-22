# xruntime 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xruntime/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `442` 个函数、`125` 个常量或宏、
`68` 个公共类型。

## `extlibs/xruntime/include/xrt/runtime_call.h`

[查看带契约注释的公共头](../../include/xrt/runtime_call.h)

### 函数 (21)

- `xrtCallFrameArgument`
- `xrtCallFrameKeyword`
- `xrtCallFrameParameter`
- `xrtCallFrameValidate`
- `xrtCallResultClear`
- `xrtCallResultCount`
- `xrtCallResultGet`
- `xrtCallResultInit`
- `xrtCallResultMove`
- `xrtCallResultPush`
- `xrtCallResultPushTake`
- `xrtCallResultSet`
- `xrtCallResultSetTake`
- `xrtCallResultUnit`
- `xrtCallableCreate`
- `xrtCallableInvoke`
- `xrtCallableRef`
- `xrtCallableSignature`
- `xrtCallableSignatureId`
- `xrtCallableUnref`
- `xrtTypeCallable`

### 常量与宏 (8)

- `XCALL_ERROR_CALLABLE`
- `XCALL_ERROR_ENTRY`
- `XCALL_ERROR_FRAME`
- `XCALL_ERROR_REFERENCE`
- `XCALL_ERROR_RESULT`
- `XCALL_ERROR_SIGNATURE`
- `XRT_CALL_RESULT_INIT`
- `XRT_CALL_RESULT_INLINE_COUNT`

### 类型 (8)

- `xcallerror`
- `xrtcallable`
- `xrtcalldrop`
- `xrtcallframe`
- `xrtcallproc`
- `xrtcallresult`
- `xrtfunctionsig`
- `xrttype`

## `extlibs/xruntime/include/xrt/runtime_convert.h`

[查看带契约注释的公共头](../../include/xrt/runtime_convert.h)

### 函数 (6)

- `xrtTypeCanConvert`
- `xrtTypeCanWiden`
- `xrtTypeConvert`
- `xrtTypeFormat`
- `xrtTypeToString`
- `xrtValueConvertTo`

### 常量与宏 (9)

- `XTYPE_CONVERT_ERROR_ARGUMENT`
- `XTYPE_CONVERT_ERROR_MODE`
- `XTYPE_CONVERT_ERROR_OPERATION`
- `XTYPE_CONVERT_ERROR_PARSE`
- `XTYPE_CONVERT_ERROR_RANGE`
- `XTYPE_CONVERT_ERROR_TYPE`
- `XTYPE_CONVERT_EXACT`
- `XTYPE_CONVERT_EXPLICIT`
- `XTYPE_CONVERT_WIDEN`

### 类型 (3)

- `xrttypewriter`
- `xtypeconverterror`
- `xtypeconvertmode`

## `extlibs/xruntime/include/xrt/runtime_field.h`

[查看带契约注释的公共头](../../include/xrt/runtime_field.h)

### 函数 (40)

- `xrtDynamicFieldsCapacity`
- `xrtDynamicFieldsClear`
- `xrtDynamicFieldsClone`
- `xrtDynamicFieldsCopy`
- `xrtDynamicFieldsCount`
- `xrtDynamicFieldsCreate`
- `xrtDynamicFieldsFromValue`
- `xrtDynamicFieldsGet`
- `xrtDynamicFieldsGetRef`
- `xrtDynamicFieldsHas`
- `xrtDynamicFieldsItems`
- `xrtDynamicFieldsIterBegin`
- `xrtDynamicFieldsIterEnd`
- `xrtDynamicFieldsIterNext`
- `xrtDynamicFieldsIterRBegin`
- `xrtDynamicFieldsKeys`
- `xrtDynamicFieldsMerge`
- `xrtDynamicFieldsRef`
- `xrtDynamicFieldsRemove`
- `xrtDynamicFieldsReserve`
- `xrtDynamicFieldsSet`
- `xrtDynamicFieldsSetNew`
- `xrtDynamicFieldsSetRef`
- `xrtDynamicFieldsSetRefNew`
- `xrtDynamicFieldsSetRefTake`
- `xrtDynamicFieldsSetTake`
- `xrtDynamicFieldsStoredName`
- `xrtDynamicFieldsTake`
- `xrtDynamicFieldsToValue`
- `xrtDynamicFieldsTrim`
- `xrtDynamicFieldsType`
- `xrtDynamicFieldsUnref`
- `xrtDynamicFieldsValues`
- `xrtFieldConstData`
- `xrtFieldData`
- `xrtTypeField`
- `xrtTypeFieldCount`
- `xrtTypeFieldOwner`
- `xrtTypeFieldsValidate`
- `xrtTypeFindField`

### 常量与宏 (8)

- `XDYNAMIC_FIELD_ERROR_ARGUMENT`
- `XDYNAMIC_FIELD_ERROR_OPERATION`
- `XDYNAMIC_FIELD_ERROR_STATE`
- `XDYNAMIC_FIELD_ERROR_TYPE`
- `XFIELD_ERROR_ACCESS`
- `XFIELD_ERROR_DESCRIPTOR`
- `XFIELD_ERROR_LOOKUP`
- `XRT_FIELD_FLAG_READONLY`

### 类型 (8)

- `xdynamicfielderror`
- `xfielderror`
- `xrtdynamicfielditer`
- `xrtdynamicfields`
- `xrtfielddesc`
- `xrtfieldtable`
- `xrtobject`
- `xtypeddictiter`

## `extlibs/xruntime/include/xrt/runtime_object.h`

[查看带契约注释的公共头](../../include/xrt/runtime_object.h)

### 函数 (18)

- `xrtObjectConstData`
- `xrtObjectCreate`
- `xrtObjectCreateSized`
- `xrtObjectData`
- `xrtObjectRef`
- `xrtObjectRefCount`
- `xrtObjectSize`
- `xrtObjectType`
- `xrtObjectUnique`
- `xrtObjectUnref`
- `xrtObjectValueOps`
- `xrtWeakCopy`
- `xrtWeakExpired`
- `xrtWeakInit`
- `xrtWeakLock`
- `xrtWeakMove`
- `xrtWeakSet`
- `xrtWeakUnit`

### 常量与宏 (5)

- `XOBJECT_ERROR_INITIALIZE`
- `XOBJECT_ERROR_REFERENCE`
- `XOBJECT_ERROR_SIZE`
- `XOBJECT_ERROR_TYPE`
- `XOBJECT_ERROR_WEAK`

### 类型 (3)

- `xobjecterror`
- `xrttypeops`
- `xrtweak`

## `extlibs/xruntime/include/xrt/runtime_object_graph.h`

[查看带契约注释的公共头](../../include/xrt/runtime_object_graph.h)

### 函数 (8)

- `xrtObjectGraphCollect`
- `xrtObjectGraphCollectRoots`
- `xrtObjectGraphContains`
- `xrtObjectGraphCount`
- `xrtObjectGraphCreate`
- `xrtObjectGraphDestroy`
- `xrtObjectGraphTrack`
- `xrtObjectGraphUntrack`

### 常量与宏 (5)

- `XOBJECT_GRAPH_ERROR_ARGUMENT`
- `XOBJECT_GRAPH_ERROR_ROOTS`
- `XOBJECT_GRAPH_ERROR_STATE`
- `XOBJECT_GRAPH_ERROR_TRACE`
- `XOBJECT_GRAPH_ERROR_TRACK`

### 类型 (5)

- `xobjectgrapherror`
- `xrtobjectgraph`
- `xrtobjectgraphresult`
- `xrtobjectrootproc`
- `xrtobjectvisitor`

## `extlibs/xruntime/include/xrt/runtime_type.h`

[查看带契约注释的公共头](../../include/xrt/runtime_type.h)

### 函数 (60)

- `xrtEnumFindName`
- `xrtEnumFindTag`
- `xrtEnumValidate`
- `xrtFunctionSigId`
- `xrtFunctionSigValidate`
- `xrtProtocolRegistryAdd`
- `xrtProtocolRegistryAt`
- `xrtProtocolRegistryCount`
- `xrtProtocolRegistryCreate`
- `xrtProtocolRegistryDestroy`
- `xrtProtocolRegistryFind`
- `xrtProtocolRegistryRemove`
- `xrtProtocolValidate`
- `xrtProtocolWitnessFind`
- `xrtProtocolWitnessValidate`
- `xrtTypeArgument`
- `xrtTypeBool`
- `xrtTypeBool32`
- `xrtTypeCloneValue`
- `xrtTypeCompareValue`
- `xrtTypeCopyValue`
- `xrtTypeDropInstance`
- `xrtTypeDropValue`
- `xrtTypeFindMethod`
- `xrtTypeFloat32`
- `xrtTypeFloat64`
- `xrtTypeHashValue`
- `xrtTypeId`
- `xrtTypeInitInstance`
- `xrtTypeInitValue`
- `xrtTypeInt16`
- `xrtTypeInt32`
- `xrtTypeInt64`
- `xrtTypeInt8`
- `xrtTypeIsA`
- `xrtTypeIsComparable`
- `xrtTypeIsCopyable`
- `xrtTypeIsHashable`
- `xrtTypeIsRelocatable`
- `xrtTypeMoveValue`
- `xrtTypeNull`
- `xrtTypePointer`
- `xrtTypeRegistryAdd`
- `xrtTypeRegistryAt`
- `xrtTypeRegistryCount`
- `xrtTypeRegistryCreate`
- `xrtTypeRegistryDestroy`
- `xrtTypeRegistryFindId`
- `xrtTypeRegistryFindName`
- `xrtTypeRegistryRemove`
- `xrtTypeSame`
- `xrtTypeTime`
- `xrtTypeTraceInstance`
- `xrtTypeTraceValue`
- `xrtTypeType`
- `xrtTypeUInt16`
- `xrtTypeUInt32`
- `xrtTypeUInt64`
- `xrtTypeUInt8`
- `xrtTypeValidate`

### 常量与宏 (47)

- `XRT_FUNCTION_FLAG_KWARGS`
- `XRT_FUNCTION_FLAG_VARARGS`
- `XRT_METHOD_FLAG_FINAL`
- `XRT_METHOD_FLAG_STATIC`
- `XRT_METHOD_FLAG_VIRTUAL`
- `XRT_PARAM_BYREF`
- `XRT_PARAM_BYVAL`
- `XRT_PARAM_DEFAULT`
- `XRT_PARAM_FLAG_NAMED_ONLY`
- `XRT_PARAM_FLAG_OPTIONAL`
- `XRT_TYPE_ARRAY`
- `XRT_TYPE_BOOL`
- `XRT_TYPE_BYTES`
- `XRT_TYPE_CALLABLE`
- `XRT_TYPE_CLASS`
- `XRT_TYPE_DICT`
- `XRT_TYPE_ENUM`
- `XRT_TYPE_FLAG_COPYABLE`
- `XRT_TYPE_FLAG_FINAL`
- `XRT_TYPE_FLAG_NULLABLE`
- `XRT_TYPE_FLAG_REFERENCE`
- `XRT_TYPE_FLAG_RELOCATABLE`
- `XRT_TYPE_FLAG_TRIVIAL_COPY`
- `XRT_TYPE_FLAG_TRIVIAL_DROP`
- `XRT_TYPE_FLOAT`
- `XRT_TYPE_FUTURE`
- `XRT_TYPE_HANDLE`
- `XRT_TYPE_INVALID`
- `XRT_TYPE_LIST`
- `XRT_TYPE_NULL`
- `XRT_TYPE_OPTIONAL`
- `XRT_TYPE_POINTER`
- `XRT_TYPE_PROTOCOL`
- `XRT_TYPE_RECORD`
- `XRT_TYPE_SET`
- `XRT_TYPE_SIGNED_INT`
- `XRT_TYPE_STRING`
- `XRT_TYPE_TIME`
- `XRT_TYPE_TYPE`
- `XRT_TYPE_UNSIGNED_INT`
- `XRT_TYPE_WEAK`
- `XTYPE_ERROR_DESCRIPTOR`
- `XTYPE_ERROR_ENUM`
- `XTYPE_ERROR_OPERATION`
- `XTYPE_ERROR_PROTOCOL`
- `XTYPE_ERROR_REGISTRY`
- `XTYPE_ERROR_SIGNATURE`

### 类型 (15)

- `xrtenum`
- `xrtenumvariant`
- `xrtinstanceops`
- `xrtmethoddesc`
- `xrtmethodtable`
- `xrtparamdesc`
- `xrtparammode`
- `xrtprotocol`
- `xrtprotocolentry`
- `xrtprotocolregistry`
- `xrtprotocolrequirement`
- `xrtprotocolwitness`
- `xrttypekind`
- `xrttyperegistry`
- `xtypeerror`

## `extlibs/xruntime/include/xrt/runtime_type_future.h`

[查看带契约注释的公共头](../../include/xrt/runtime_type_future.h)

### 函数 (1)

- `xrtTypeFuture`

## `extlibs/xruntime/include/xrt/runtime_type_string.h`

[查看带契约注释的公共头](../../include/xrt/runtime_type_string.h)

### 函数 (1)

- `xrtTypeString`

## `extlibs/xruntime/include/xrt/runtime_value.h`

[查看带契约注释的公共头](../../include/xrt/runtime_value.h)

### 函数 (26)

- `xrtObjectGraphCollectValueRoot`
- `xrtObjectGraphCollectValueRoots`
- `xrtProgressCallInit`
- `xrtProgressCallInvoke`
- `xrtTypeValue`
- `xrtValueCallable`
- `xrtValueCallableSignature`
- `xrtValueCallableTake`
- `xrtValueFuture`
- `xrtValueFutureTake`
- `xrtValueGetCallable`
- `xrtValueGetFuture`
- `xrtValueGetRuntimeObject`
- `xrtValueGetWeak`
- `xrtValueInvoke`
- `xrtValueIsCallable`
- `xrtValueIsFuture`
- `xrtValueIsRuntimeObject`
- `xrtValueIsWeak`
- `xrtValueRuntimeObject`
- `xrtValueRuntimeObjectTake`
- `xrtValueTraceRuntimeObjects`
- `xrtValueWeak`
- `xrtValueWeakExpired`
- `xrtValueWeakLock`
- `xrtValueWeakTake`

### 常量与宏 (8)

- `XRUNTIME_VALUE_ERROR_CALLABLE`
- `XRUNTIME_VALUE_ERROR_FUTURE`
- `XRUNTIME_VALUE_ERROR_OBJECT`
- `XRUNTIME_VALUE_ERROR_OWNERSHIP`
- `XRUNTIME_VALUE_ERROR_ROOTS`
- `XRUNTIME_VALUE_ERROR_TRACE`
- `XRUNTIME_VALUE_ERROR_TYPE`
- `XRUNTIME_VALUE_ERROR_WEAK`

### 类型 (2)

- `xrtprogress`
- `xrtprogresscall`

## `extlibs/xruntime/include/xrt/typed_array.h`

[查看带契约注释的公共头](../../include/xrt/typed_array.h)

### 函数 (31)

- `xrtTypedArrayAppend`
- `xrtTypedArrayCapacity`
- `xrtTypedArrayClear`
- `xrtTypedArrayClone`
- `xrtTypedArrayConcat`
- `xrtTypedArrayConstData`
- `xrtTypedArrayConstGet`
- `xrtTypedArrayContains`
- `xrtTypedArrayCount`
- `xrtTypedArrayCreate`
- `xrtTypedArrayData`
- `xrtTypedArrayDestroy`
- `xrtTypedArrayEquals`
- `xrtTypedArrayFind`
- `xrtTypedArrayGet`
- `xrtTypedArrayInit`
- `xrtTypedArrayInsert`
- `xrtTypedArrayInstanceOps`
- `xrtTypedArrayItemType`
- `xrtTypedArrayPop`
- `xrtTypedArrayPush`
- `xrtTypedArrayRemove`
- `xrtTypedArrayReserve`
- `xrtTypedArrayResize`
- `xrtTypedArrayReverse`
- `xrtTypedArraySet`
- `xrtTypedArraySwap`
- `xrtTypedArrayTake`
- `xrtTypedArrayTrim`
- `xrtTypedArrayTypeValidate`
- `xrtTypedArrayUnit`

### 常量与宏 (5)

- `XTYPED_ARRAY_ERROR_ARGUMENT`
- `XTYPED_ARRAY_ERROR_OPERATION`
- `XTYPED_ARRAY_ERROR_RANGE`
- `XTYPED_ARRAY_ERROR_STATE`
- `XTYPED_ARRAY_ERROR_TYPE`

### 类型 (2)

- `xtypedarray`
- `xtypedarrayerror`

## `extlibs/xruntime/include/xrt/typed_dict.h`

[查看带契约注释的公共头](../../include/xrt/typed_dict.h)

### 函数 (30)

- `xrtTypedDictAt`
- `xrtTypedDictCapacity`
- `xrtTypedDictClear`
- `xrtTypedDictClone`
- `xrtTypedDictConstAt`
- `xrtTypedDictConstGet`
- `xrtTypedDictCount`
- `xrtTypedDictCreate`
- `xrtTypedDictDestroy`
- `xrtTypedDictEquals`
- `xrtTypedDictGet`
- `xrtTypedDictGetOrAdd`
- `xrtTypedDictHas`
- `xrtTypedDictInit`
- `xrtTypedDictInstanceOps`
- `xrtTypedDictItemType`
- `xrtTypedDictIterBegin`
- `xrtTypedDictIterEnd`
- `xrtTypedDictIterNext`
- `xrtTypedDictIterRBegin`
- `xrtTypedDictMerge`
- `xrtTypedDictRemove`
- `xrtTypedDictReserve`
- `xrtTypedDictSet`
- `xrtTypedDictSetTake`
- `xrtTypedDictStoredKey`
- `xrtTypedDictTake`
- `xrtTypedDictTrim`
- `xrtTypedDictTypeValidate`
- `xrtTypedDictUnit`

### 常量与宏 (5)

- `XTYPED_DICT_ERROR_ARGUMENT`
- `XTYPED_DICT_ERROR_OPERATION`
- `XTYPED_DICT_ERROR_RANGE`
- `XTYPED_DICT_ERROR_STATE`
- `XTYPED_DICT_ERROR_TYPE`

### 类型 (2)

- `xtypeddict`
- `xtypeddicterror`

## `extlibs/xruntime/include/xrt/typed_list.h`

[查看带契约注释的公共头](../../include/xrt/typed_list.h)

### 函数 (30)

- `xrtTypedListAppend`
- `xrtTypedListAt`
- `xrtTypedListClear`
- `xrtTypedListClone`
- `xrtTypedListConstAt`
- `xrtTypedListConstGet`
- `xrtTypedListContains`
- `xrtTypedListCount`
- `xrtTypedListCreate`
- `xrtTypedListDestroy`
- `xrtTypedListEquals`
- `xrtTypedListFind`
- `xrtTypedListGet`
- `xrtTypedListHas`
- `xrtTypedListInit`
- `xrtTypedListInstanceOps`
- `xrtTypedListItemType`
- `xrtTypedListIterBegin`
- `xrtTypedListIterEnd`
- `xrtTypedListIterFrom`
- `xrtTypedListIterNext`
- `xrtTypedListIterRBegin`
- `xrtTypedListIterRFrom`
- `xrtTypedListMerge`
- `xrtTypedListRemove`
- `xrtTypedListSet`
- `xrtTypedListTake`
- `xrtTypedListTrim`
- `xrtTypedListTypeValidate`
- `xrtTypedListUnit`

### 常量与宏 (5)

- `XTYPED_LIST_ERROR_ARGUMENT`
- `XTYPED_LIST_ERROR_KEY`
- `XTYPED_LIST_ERROR_OPERATION`
- `XTYPED_LIST_ERROR_STATE`
- `XTYPED_LIST_ERROR_TYPE`

### 类型 (3)

- `xtypedlist`
- `xtypedlisterror`
- `xtypedlistiter`

## `extlibs/xruntime/include/xrt/typed_queue.h`

[查看带契约注释的公共头](../../include/xrt/typed_queue.h)

### 函数 (54)

- `xrtTypedMPMCQueueCapacity`
- `xrtTypedMPMCQueueClose`
- `xrtTypedMPMCQueueCount`
- `xrtTypedMPMCQueueCreate`
- `xrtTypedMPMCQueueDestroy`
- `xrtTypedMPMCQueueInit`
- `xrtTypedMPMCQueueInstanceOps`
- `xrtTypedMPMCQueueIsClosed`
- `xrtTypedMPMCQueueIsDrained`
- `xrtTypedMPMCQueueItemType`
- `xrtTypedMPMCQueuePopBatch`
- `xrtTypedMPMCQueuePushBatch`
- `xrtTypedMPMCQueueReset`
- `xrtTypedMPMCQueueTryPop`
- `xrtTypedMPMCQueueTryPush`
- `xrtTypedMPMCQueueTryPushTake`
- `xrtTypedMPMCQueueTypeValidate`
- `xrtTypedMPMCQueueUnit`
- `xrtTypedMPSCQueueCapacity`
- `xrtTypedMPSCQueueClose`
- `xrtTypedMPSCQueueCount`
- `xrtTypedMPSCQueueCreate`
- `xrtTypedMPSCQueueDestroy`
- `xrtTypedMPSCQueueInit`
- `xrtTypedMPSCQueueInstanceOps`
- `xrtTypedMPSCQueueIsClosed`
- `xrtTypedMPSCQueueIsDrained`
- `xrtTypedMPSCQueueItemType`
- `xrtTypedMPSCQueuePopBatch`
- `xrtTypedMPSCQueuePushBatch`
- `xrtTypedMPSCQueueReset`
- `xrtTypedMPSCQueueTryPop`
- `xrtTypedMPSCQueueTryPush`
- `xrtTypedMPSCQueueTryPushTake`
- `xrtTypedMPSCQueueTypeValidate`
- `xrtTypedMPSCQueueUnit`
- `xrtTypedSPSCQueueCapacity`
- `xrtTypedSPSCQueueClose`
- `xrtTypedSPSCQueueCount`
- `xrtTypedSPSCQueueCreate`
- `xrtTypedSPSCQueueDestroy`
- `xrtTypedSPSCQueueInit`
- `xrtTypedSPSCQueueInstanceOps`
- `xrtTypedSPSCQueueIsClosed`
- `xrtTypedSPSCQueueIsDrained`
- `xrtTypedSPSCQueueItemType`
- `xrtTypedSPSCQueuePopBatch`
- `xrtTypedSPSCQueuePushBatch`
- `xrtTypedSPSCQueueReset`
- `xrtTypedSPSCQueueTryPop`
- `xrtTypedSPSCQueueTryPush`
- `xrtTypedSPSCQueueTryPushTake`
- `xrtTypedSPSCQueueTypeValidate`
- `xrtTypedSPSCQueueUnit`

### 常量与宏 (5)

- `XTYPED_QUEUE_ERROR_ARGUMENT`
- `XTYPED_QUEUE_ERROR_LAYOUT`
- `XTYPED_QUEUE_ERROR_OPERATION`
- `XTYPED_QUEUE_ERROR_STATE`
- `XTYPED_QUEUE_ERROR_TYPE`

### 类型 (6)

- `xtypedmpmcqueue`
- `xtypedmpscqueue`
- `xtypedqueuecore`
- `xtypedqueueerror`
- `xtypedqueuemeta`
- `xtypedspscqueue`

## `extlibs/xruntime/include/xrt/typed_set.h`

[查看带契约注释的公共头](../../include/xrt/typed_set.h)

### 函数 (33)

- `xrtTypedSetAdd`
- `xrtTypedSetAt`
- `xrtTypedSetCapacity`
- `xrtTypedSetClear`
- `xrtTypedSetClone`
- `xrtTypedSetCount`
- `xrtTypedSetCreate`
- `xrtTypedSetDestroy`
- `xrtTypedSetDifference`
- `xrtTypedSetEquals`
- `xrtTypedSetGet`
- `xrtTypedSetGetOrAdd`
- `xrtTypedSetHas`
- `xrtTypedSetInit`
- `xrtTypedSetInstanceOps`
- `xrtTypedSetIntersection`
- `xrtTypedSetIsDisjoint`
- `xrtTypedSetIsSubset`
- `xrtTypedSetIsSuperset`
- `xrtTypedSetItemType`
- `xrtTypedSetIterBegin`
- `xrtTypedSetIterEnd`
- `xrtTypedSetIterNext`
- `xrtTypedSetIterRBegin`
- `xrtTypedSetMerge`
- `xrtTypedSetRemove`
- `xrtTypedSetReserve`
- `xrtTypedSetSymmetricDifference`
- `xrtTypedSetTake`
- `xrtTypedSetTrim`
- `xrtTypedSetTypeValidate`
- `xrtTypedSetUnion`
- `xrtTypedSetUnit`

### 常量与宏 (5)

- `XTYPED_SET_ERROR_ARGUMENT`
- `XTYPED_SET_ERROR_OPERATION`
- `XTYPED_SET_ERROR_RANGE`
- `XTYPED_SET_ERROR_STATE`
- `XTYPED_SET_ERROR_TYPE`

### 类型 (3)

- `xtypedset`
- `xtypedseterror`
- `xtypedsetiter`

## `extlibs/xruntime/include/xrt/typed_stack.h`

[查看带契约注释的公共头](../../include/xrt/typed_stack.h)

### 函数 (18)

- `xrtTypedStackCapacity`
- `xrtTypedStackClear`
- `xrtTypedStackClone`
- `xrtTypedStackConstPeek`
- `xrtTypedStackConstTop`
- `xrtTypedStackCount`
- `xrtTypedStackCreate`
- `xrtTypedStackDestroy`
- `xrtTypedStackEquals`
- `xrtTypedStackInit`
- `xrtTypedStackItemType`
- `xrtTypedStackPeek`
- `xrtTypedStackPop`
- `xrtTypedStackPush`
- `xrtTypedStackReserve`
- `xrtTypedStackTop`
- `xrtTypedStackTrim`
- `xrtTypedStackUnit`

### 类型 (1)

- `xtypedstack`

## `extlibs/xruntime/include/xrt/typed_tree.h`

[查看带契约注释的公共头](../../include/xrt/typed_tree.h)

### 函数 (33)

- `xrtTypedTreeClear`
- `xrtTypedTreeClone`
- `xrtTypedTreeConstGet`
- `xrtTypedTreeCount`
- `xrtTypedTreeCreate`
- `xrtTypedTreeDestroy`
- `xrtTypedTreeEquals`
- `xrtTypedTreeFirst`
- `xrtTypedTreeGet`
- `xrtTypedTreeGetOrAdd`
- `xrtTypedTreeHas`
- `xrtTypedTreeInit`
- `xrtTypedTreeInstanceOps`
- `xrtTypedTreeIterBegin`
- `xrtTypedTreeIterEnd`
- `xrtTypedTreeIterFrom`
- `xrtTypedTreeIterNext`
- `xrtTypedTreeIterRBegin`
- `xrtTypedTreeIterRFrom`
- `xrtTypedTreeKeyType`
- `xrtTypedTreeLast`
- `xrtTypedTreeLowerBound`
- `xrtTypedTreeMerge`
- `xrtTypedTreeRemove`
- `xrtTypedTreeSet`
- `xrtTypedTreeSetTake`
- `xrtTypedTreeStoredKey`
- `xrtTypedTreeTake`
- `xrtTypedTreeTrim`
- `xrtTypedTreeTypeValidate`
- `xrtTypedTreeUnit`
- `xrtTypedTreeUpperBound`
- `xrtTypedTreeValueType`

### 常量与宏 (5)

- `XTYPED_TREE_ERROR_ARGUMENT`
- `XTYPED_TREE_ERROR_LAYOUT`
- `XTYPED_TREE_ERROR_OPERATION`
- `XTYPED_TREE_ERROR_STATE`
- `XTYPED_TREE_ERROR_TYPE`

### 类型 (3)

- `xtypedtree`
- `xtypedtreeerror`
- `xtypedtreeiter`

## `extlibs/xruntime/include/xrt/typed_value.h`

[查看带契约注释的公共头](../../include/xrt/typed_value.h)

### 函数 (32)

- `xrtTypedArrayContainsValue`
- `xrtTypedArrayFindValue`
- `xrtTypedArrayFromValue`
- `xrtTypedArrayGetValue`
- `xrtTypedArrayInsertValue`
- `xrtTypedArrayPopValue`
- `xrtTypedArrayPushValue`
- `xrtTypedArraySetValue`
- `xrtTypedArrayTakeValue`
- `xrtTypedArrayToValue`
- `xrtTypedDictFromValue`
- `xrtTypedDictGetValue`
- `xrtTypedDictSetValue`
- `xrtTypedDictTakeValue`
- `xrtTypedDictToValue`
- `xrtTypedListAppendValue`
- `xrtTypedListContainsValue`
- `xrtTypedListFindValue`
- `xrtTypedListFromValue`
- `xrtTypedListGetValue`
- `xrtTypedListSetValue`
- `xrtTypedListTakeValue`
- `xrtTypedListToValue`
- `xrtTypedSetAddValue`
- `xrtTypedSetFromValue`
- `xrtTypedSetGetValue`
- `xrtTypedSetHasValue`
- `xrtTypedSetRemoveValue`
- `xrtTypedSetTakeValue`
- `xrtTypedSetToValue`
- `xrtValueFromTyped`
- `xrtValueToTyped`

### 常量与宏 (5)

- `XTYPED_VALUE_ERROR_ARGUMENT`
- `XTYPED_VALUE_ERROR_CONTAINER`
- `XTYPED_VALUE_ERROR_CONVERT`
- `XTYPED_VALUE_ERROR_RANGE`
- `XTYPED_VALUE_ERROR_TYPE`

### 类型 (4)

- `xtypedvalueerror`
- `xvalueconverter`
- `xvaluefromtyped`
- `xvaluetotyped`
