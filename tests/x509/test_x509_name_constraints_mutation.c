#include "../test.h"
#include "../fixtures/x509_name_constraints_vectors.h"



/* 检查一个变异输入的解析原子性和子树游标终止性。 */
static void testNameConstraintsMutationOne(
	const uint8* pDer,
	size_t iSize,
	size_t* pParsed,
	size_t* pErrors
)
{
	xx509nameconstraints Constraints;
	xx509nameconstraints Before;

	memset(&Constraints, 0xA5, sizeof(Constraints));
	Before = Constraints;
	if ( !xrtX509NameConstraintsParse(
		(xbytesview) { pDer, iSize }, &Constraints
	) ) {
		testRequire(memcmp(
			&Constraints, &Before, sizeof(Constraints)
		) == 0, "mutated NameConstraints changed failed output");
		(*pErrors)++;
		return;
	}
	(*pParsed)++;
	for ( size_t iSet = 0; iSet < 2u; iSet++ ) {
		xx509subtreecursor Cursor;
		xx509subtree Subtree;
		xx509subtree BeforeSubtree;
		xx509result Result = X509_DONE;
		bool bPresent = iSet == 0 ?
			Constraints.HasPermitted : Constraints.HasExcluded;

		if ( !bPresent ) {
			continue;
		}
		Cursor = iSet == 0 ? Constraints.Permitted : Constraints.Excluded;
		for ( size_t i = 0; i < 64u; i++ ) {
			memset(&Subtree, 0xA5, sizeof(Subtree));
			BeforeSubtree = Subtree;
			Result = xrtX509SubtreeRead(&Cursor, &Subtree);
			if ( Result == X509_VALUE ) {
				continue;
			}
			testRequire(memcmp(
				&Subtree, &BeforeSubtree, sizeof(Subtree)
			) == 0, "mutated GeneralSubtree changed terminal output");
			break;
		}
		testRequire(Result != X509_VALUE,
			"mutated GeneralSubtrees cursor did not terminate");
	}
}



/* 对完整向量执行单比特和确定性多字节变异。 */
int main(void)
{
	uint8 Mutated[sizeof(X509_NAME_CONSTRAINTS_MIXED)];
	uint32 iState = UINT32_C(0xA17C9E35);
	size_t iParsed = 0;
	size_t iErrors = 0;
	size_t iCases = 0;

	for ( size_t i = 0; i < sizeof(Mutated); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(
				Mutated, X509_NAME_CONSTRAINTS_MIXED, sizeof(Mutated)
			);
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			testNameConstraintsMutationOne(
				Mutated, sizeof(Mutated), &iParsed, &iErrors
			);
			iCases++;
		}
	}
	for ( size_t i = 0; i < 2048u; i++ ) {
		memcpy(Mutated, X509_NAME_CONSTRAINTS_MIXED, sizeof(Mutated));
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Mutated[iState % sizeof(Mutated)] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		testNameConstraintsMutationOne(
			Mutated, sizeof(Mutated), &iParsed, &iErrors
		);
		iCases++;
	}
	testRequire((iParsed != 0) && (iErrors != 0),
		"NameConstraints mutation corpus missed a result class");
	printf(
		"[PASS] x509_name_constraints_mutation cases=%zu parsed=%zu errors=%zu\n",
		iCases, iParsed, iErrors
	);
	return 0;
}
