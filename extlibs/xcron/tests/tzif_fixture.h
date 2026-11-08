#ifndef XCRON_TEST_TZIF_FIXTURE_H
#define XCRON_TEST_TZIF_FIXTURE_H
#ifndef XCRON_TEST_TZIF_BYTES
#define XCRON_TEST_TZIF_BYTES 4096
#endif
typedef struct tzifkind {int32 Offset;bool DST;cstr Name;} tzifkind;
typedef struct tzifblob {uint8 Data[XCRON_TEST_TZIF_BYTES];size_t Size,Header,Times,Indices,Types,Names,End;} tzifblob;
static inline void tzifU32(uint8* P,uint32 U){for(unsigned I=0;I<4;++I)P[I]=(uint8)(U>>(24-8*I));}
static inline void tzifTime(uint8* P,int64 S,unsigned Width){uint64 U=(uint64)S;for(unsigned I=0;I<Width;++I)P[I]=(uint8)(U>>((Width-I-1)*8));}
static inline void tzifBuild(tzifblob* B,char Version,const int64* Times,const uint8* Indices,size_t Count,const tzifkind* Types,size_t TypesCount,cstr Footer)
{
    memset(B,0,sizeof(*B));unsigned Width=Version?8:4;size_t At=0;
    if(Version){memcpy(B->Data,"TZif",4);B->Data[4]=(uint8)Version;tzifU32(B->Data+36,1);tzifU32(B->Data+40,1);At=51;}
    B->Header=At;uint8* P=B->Data+At;memcpy(P,"TZif",4);P[4]=(uint8)Version;
    size_t Chars=0;for(size_t I=0;I<TypesCount;++I)Chars+=strlen(Types[I].Name)+1;
    tzifU32(P+32,(uint32)Count);tzifU32(P+36,(uint32)TypesCount);tzifU32(P+40,(uint32)Chars);
    B->Times=At+44;B->Indices=B->Times+Count*Width;B->Types=B->Indices+Count;B->Names=B->Types+TypesCount*6;B->End=B->Names+Chars;size_t Name=0;
    testRequire(B->End+strlen(Footer)+2<sizeof(B->Data),"bounded TZif fixture");
    for(size_t I=0;I<Count;++I){tzifTime(B->Data+B->Times+I*Width,Times[I],Width);B->Data[B->Indices+I]=Indices[I];}
    for(size_t I=0;I<TypesCount;++I){P=B->Data+B->Types+I*6;tzifU32(P,(uint32)Types[I].Offset);P[4]=(uint8)Types[I].DST;testRequire(Name<=255,"fixture designation index");P[5]=(uint8)Name;size_t N=strlen(Types[I].Name)+1;memcpy(B->Data+B->Names+Name,Types[I].Name,N);Name+=N;}
    B->Size=B->End;if(Version){B->Data[B->Size++]='\n';memcpy(B->Data+B->Size,Footer,strlen(Footer));B->Size+=strlen(Footer);B->Data[B->Size++]='\n';}
}
#endif
